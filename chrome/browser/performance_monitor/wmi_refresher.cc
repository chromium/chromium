// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/wmi_refresher.h"

#include <Wbemidl.h>

#include <algorithm>
#include <limits>
#include <vector>

#include "base/threading/scoped_blocking_call.h"
#include "base/win/com_init_util.h"

namespace performance_monitor {
namespace win {

namespace {

using Microsoft::WRL::ComPtr;

// Helper function to read a property from a IWbemObjectAccess object.
bool GetPropertyHandle(base::Optional<long>* handle,
                       Microsoft::WRL::ComPtr<IWbemObjectAccess> enum_object,
                       LPCWSTR property_name) {
  DCHECK(handle);
  CIMTYPE prop_type = 0;
  long handle_value = 0;
  if (FAILED(enum_object->GetPropertyHandle(property_name, &prop_type,
                                            &handle_value))) {
    return false;
  }
  *handle = handle_value;
  return true;
}

// Compute the delta between 2 observations. The counters might wrap once they
// reach std::numeric_limits<DWORD>::max().
DWORD CalculateObservationDelta(DWORD ref_value, DWORD new_value) {
  if (new_value > ref_value) {
    return new_value - ref_value;
  } else {
    return std::numeric_limits<DWORD>::max() - ref_value + new_value;
  }
}

}  // namespace

enum class WMIRefresher::InitStatus {
  kInitStatusOk,
  kLocalWMIConnectionError,
  kRefresherCreationError,
  kRefresherConfigError,
  kRefresherAddEnumError,
  kMaxValue = kRefresherAddEnumError
};

enum class WMIRefresher::RefreshStatus {
  kRefreshOk,
  kRefreshFailed,
  kGetObjectFailed,
  kGetPropertyHandleFailed,
  kReadValueFailed,
  kGetObjectReturnedNoObjects,
  kMaxValue = kGetObjectReturnedNoObjects
};

WMIRefresher::WMIRefresher() : initialized_called_(false) {
  // This object might be created on a sequence different than the one on which
  // it'll be used.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WMIRefresher::~WMIRefresher() = default;

bool WMIRefresher::InitializeDiskIdleTimeConfig() {
  DCHECK(!initialized_called_);
  WMIRefresher::InitStatus result = WMIRefresher::InitStatus::kInitStatusOk;
  InitializeDiskIdleTimeConfigImpl(&result);

  initialized_called_ = true;

  return result == InitStatus::kInitStatusOk;
}

void WMIRefresher::InitializeDiskIdleTimeConfigImpl(
    WMIRefresher::InitStatus* res) {
  DCHECK(res);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // This assumes that CoInitialize(Ex) has already been called on this thread.
  AssertComApartmentType(base::win::ComApartmentType::MTA);

  if (!base::win::CreateLocalWmiConnection(true /* set_blanket */,
                                           &wmi_services_)) {
    LOG(ERROR) << "Unable to create the local WMI connection";
    *res = InitStatus::kLocalWMIConnectionError;
    return;
  }

  HRESULT hr = S_OK;
  // Creates the WMI refresher interface.
  if (FAILED(hr = ::CoCreateInstance(CLSID_WbemRefresher, nullptr,
                                     CLSCTX_INPROC_SERVER,
                                     IID_PPV_ARGS(&wmi_refresher_)))) {
    LOG(ERROR) << "Unable to create the WMI refresher interface.";
    *res = InitStatus::kRefresherCreationError;
    return;
  }

  // Get the interface to configure the refresher.
  ComPtr<IWbemConfigureRefresher> wmi_refresher_config;
  hr = wmi_refresher_.As(&wmi_refresher_config);
  if (FAILED(hr)) {
    LOG(ERROR) << "Unable to configure the WMI refresher.";
    *res = InitStatus::kRefresherConfigError;
    return;
  }

  long wmi_refresher_enum_id = 0;
  // Add the enumerator for the disk performance data.
  hr = wmi_refresher_config->AddEnum(
      wmi_services_.Get(), L"Win32_PerfRawData_PerfDisk_PhysicalDisk", 0,
      nullptr, &wmi_refresher_enum_, &wmi_refresher_enum_id);
  if (FAILED(hr)) {
    LOG(ERROR)
        << "Unable to add the Win32_PerfRawData_PerfDisk_PhysicalDisk enum.";
    *res = InitStatus::kRefresherAddEnumError;
    return;
  }

  *res = InitStatus::kInitStatusOk;

  refresh_ready_ = true;
}

base::Optional<float> WMIRefresher::RefreshAndGetDiskIdleTimeInPercent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_called_);
  RefreshStatus result = WMIRefresher::RefreshStatus::kRefreshOk;
  auto idle_time = RefreshAndGetDiskIdleTimeInPercentImpl(&result);
  return idle_time;
}

base::Optional<float> WMIRefresher::RefreshAndGetDiskIdleTimeInPercentImpl(
    WMIRefresher::RefreshStatus* res) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(res);
  DCHECK(refresh_ready_);
  AssertComApartmentType(base::win::ComApartmentType::MTA);

  HRESULT hr = wmi_refresher_->Refresh(WBEM_FLAG_REFRESH_AUTO_RECONNECT);
  if (FAILED(hr)) {
    LOG(ERROR) << "Error while trying to use the WMI refresher.";
    *res = RefreshStatus::kRefreshFailed;
    return base::nullopt;
  }

  // Get the objects owned by the enumerator.
  ULONG number_of_objects = 0;
  static_assert(sizeof(Microsoft::WRL::ComPtr<IWbemObjectAccess>) ==
                    sizeof(IWbemObjectAccess*),
                "This code assumes that the size of a ComPtr<T> object is "
                "equal to the size of T*.");
  std::vector<Microsoft::WRL::ComPtr<IWbemObjectAccess>>
      wmi_refresher_enum_objects;
  DCHECK_GT(wmi_refresher_enum_objects_latest_count_, 0U);
  wmi_refresher_enum_objects.resize(wmi_refresher_enum_objects_latest_count_);
  hr = wmi_refresher_enum_->GetObjects(0L, wmi_refresher_enum_objects.size(),
                                       &wmi_refresher_enum_objects[0],
                                       &number_of_objects);

  if (number_of_objects == 0U) {
    *res = RefreshStatus::kGetObjectReturnedNoObjects;
    return base::nullopt;
  }

  // The number of objects returned might change over time (e.g. when connecting
  // an external hard drive). If the number of objects returned is smaller than
  // the size of the vector then the latest(s) element(s) will be left
  // uninitialized.
  wmi_refresher_enum_objects_latest_count_ = number_of_objects;

  // Resize the buffer if necessary.
  if (hr == WBEM_E_BUFFER_TOO_SMALL &&
      number_of_objects > wmi_refresher_enum_objects.size()) {
    wmi_refresher_enum_objects.clear();
    wmi_refresher_enum_objects.resize(wmi_refresher_enum_objects_latest_count_);
    if (FAILED(hr = wmi_refresher_enum_->GetObjects(
                   0L, wmi_refresher_enum_objects.size(),
                   &wmi_refresher_enum_objects[0], &number_of_objects))) {
      *res = RefreshStatus::kGetObjectFailed;
      return base::nullopt;
    }
  }

  // Ensure that we can safely access wmi_refresher_enum_objects[0] after this.
  CHECK_GE(number_of_objects, 1U);

  // Initialize the property handles if necessary.
  if (!percent_idle_time_prop_handle_ &&
      !GetPropertyHandle(&percent_idle_time_prop_handle_,
                         wmi_refresher_enum_objects[0], L"PercentIdleTime")) {
    *res = RefreshStatus::kGetPropertyHandleFailed;
    return base::nullopt;
  }
  if (!percent_idle_time_base_prop_handle_ &&
      !GetPropertyHandle(&percent_idle_time_base_prop_handle_,
                         wmi_refresher_enum_objects[0],
                         L"PercentIdleTime_Base")) {
    *res = RefreshStatus::kGetPropertyHandleFailed;
    return base::nullopt;
  }

  // Read the property values.
  //
  // TODO(crbug.com/907635): This only looks at the first object returned, which
  // is the total idle time for all disks. Instead there should probably be two
  // values:
  //   - Idle time for the disk containing the Chrome user data directory.
  //   - Idle time for the disk hosting the pagefile.
  DWORD new_idle_time = 0;
  DWORD new_percent_idle_time_base = 0;
  if (FAILED(hr = wmi_refresher_enum_objects[0]->ReadDWORD(
                 percent_idle_time_prop_handle_.value(), &new_idle_time))) {
    *res = RefreshStatus::kReadValueFailed;
    return base::nullopt;
  }
  if (FAILED(hr = wmi_refresher_enum_objects[0]->ReadDWORD(
                 percent_idle_time_base_prop_handle_.value(),
                 &new_percent_idle_time_base))) {
    *res = RefreshStatus::kReadValueFailed;
    return base::nullopt;
  }

  base::Optional<float> idle_time;

  // Compute the delta if we have at least 2 samples.
  if (latest_percent_idle_time_val_ && latest_percent_idle_time_base_val_) {
    // Note that although this value is coming from a property called
    // "PercentIdleTime" it's not a direct percentage value. In order to get the
    // actual percentage value it's required to compute
    // (new_value - old_value) / (new_value_base - old_value_base).
    DWORD percent_idle_time_delta = CalculateObservationDelta(
        latest_percent_idle_time_val_.value(), new_idle_time);
    DWORD percent_idle_time_base_delta = CalculateObservationDelta(
        latest_percent_idle_time_base_val_.value(), new_percent_idle_time_base);

    idle_time = std::min(1.0f, static_cast<float>(percent_idle_time_delta) /
                                   percent_idle_time_base_delta);
  }

  latest_percent_idle_time_val_ = new_idle_time;
  latest_percent_idle_time_base_val_ = new_percent_idle_time_base;

  *res = RefreshStatus::kRefreshOk;
  return idle_time;
}

}  // namespace win
}  // namespace performance_monitor
