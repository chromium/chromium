// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/battery_level_provider.h"

#define INITGUID
#include <windows.h>  // Must be in front of other Windows header files.

#include <devguid.h>
#include <poclass.h>
#include <setupapi.h>
#include <winioctl.h>

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_devinfo.h"
#include "base/win/scoped_handle.h"

namespace base {
namespace {

// Returns a handle to the battery interface identified by |interface_data|, or
// nullopt if the request failed. |devices| is a device information set that
// contains battery devices information, obtained with ::SetupDiGetClassDevs().
base::win::ScopedHandle GetBatteryHandle(
    HDEVINFO devices,
    SP_DEVICE_INTERFACE_DATA* interface_data) {
  // Query size required to hold |interface_detail|.
  DWORD required_size = 0;
  ::SetupDiGetDeviceInterfaceDetail(devices, interface_data, nullptr, 0,
                                    &required_size, nullptr);
  DWORD error = ::GetLastError();
  if (error != ERROR_INSUFFICIENT_BUFFER)
    return base::win::ScopedHandle();

  // |interface_detail->DevicePath| is variable size.
  std::vector<uint8_t> raw_buf(required_size);
  auto* interface_detail =
      reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(raw_buf.data());
  interface_detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

  BOOL success = ::SetupDiGetDeviceInterfaceDetail(
      devices, interface_data, interface_detail, required_size, nullptr,
      nullptr);
  if (!success)
    return base::win::ScopedHandle();

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::win::ScopedHandle battery(
      ::CreateFile(interface_detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                   FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL, nullptr));
  return battery;
}

// Returns the current tag for `battery` handle, BATTERY_TAG_INVALID if there is
// no battery present in this interface or nullopt on retrieval error.
// See
// https://docs.microsoft.com/en-us/windows/win32/power/ioctl-battery-query-tag
absl::optional<ULONG> GetBatteryTag(HANDLE battery) {
  ULONG battery_tag = 0;
  ULONG wait = 0;
  DWORD bytes_returned = 0;
  BOOL success = ::DeviceIoControl(
      battery, IOCTL_BATTERY_QUERY_TAG, &wait, sizeof(wait), &battery_tag,
      sizeof(battery_tag), &bytes_returned, nullptr);
  if (!success) {
    if (::GetLastError() == ERROR_FILE_NOT_FOUND) {
      // No battery present in this interface.
      //
      // TODO(crbug.com/1191045): Change CHECK to DCHECK in October 2022 after
      // verifying that there are no crash reports.
      CHECK_EQ(battery_tag, static_cast<ULONG>(BATTERY_TAG_INVALID));
      return battery_tag;
    }
    // Retrieval error.
    return absl::nullopt;
  }
  return battery_tag;
}

// Returns BATTERY_INFORMATION structure containing battery information, given
// battery handle and tag, or nullopt if the request failed. Battery handle and
// tag are obtained with GetBatteryHandle() and GetBatteryTag(), respectively.
absl::optional<BATTERY_INFORMATION> GetBatteryInformation(HANDLE battery,
                                                          ULONG battery_tag) {
  BATTERY_QUERY_INFORMATION query_information = {};
  query_information.BatteryTag = battery_tag;
  query_information.InformationLevel = BatteryInformation;
  BATTERY_INFORMATION battery_information = {};
  DWORD bytes_returned;
  BOOL success = ::DeviceIoControl(
      battery, IOCTL_BATTERY_QUERY_INFORMATION, &query_information,
      sizeof(query_information), &battery_information,
      sizeof(battery_information), &bytes_returned, nullptr);
  if (!success)
    return absl::nullopt;
  return battery_information;
}

// Returns BATTERY_STATUS structure containing battery state, given battery
// handle and tag, or nullopt if the request failed. Battery handle and tag are
// obtained with GetBatteryHandle() and GetBatteryTag(), respectively.
absl::optional<BATTERY_STATUS> GetBatteryStatus(HANDLE battery,
                                                ULONG battery_tag) {
  BATTERY_WAIT_STATUS wait_status = {};
  wait_status.BatteryTag = battery_tag;
  BATTERY_STATUS battery_status;
  DWORD bytes_returned;
  BOOL success = ::DeviceIoControl(
      battery, IOCTL_BATTERY_QUERY_STATUS, &wait_status, sizeof(wait_status),
      &battery_status, sizeof(battery_status), &bytes_returned, nullptr);
  if (!success)
    return absl::nullopt;
  return battery_status;
}

}  // namespace

class BatteryLevelProviderWin : public BatteryLevelProvider {
 public:
  BatteryLevelProviderWin() = default;
  ~BatteryLevelProviderWin() override = default;

  void GetBatteryState(
      base::OnceCallback<void(const absl::optional<BatteryState>&)> callback)
      override {
    // This is run on |blocking_task_runner_| since `GetBatteryStateImpl()` has
    // blocking calls and can take several seconds to complete.
    blocking_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&BatteryLevelProviderWin::GetBatteryStateImpl),
        base::BindOnce(&BatteryLevelProviderWin::OnBatteryStateObtained,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  static absl::optional<BatteryState> GetBatteryStateImpl();

  void OnBatteryStateObtained(
      base::OnceCallback<void(const absl::optional<BatteryState>&)> callback,
      const absl::optional<BatteryState>& battery_state) {
    std::move(callback).Run(battery_state);
  }

  // TaskRunner used to run blocking `GetBatteryStateImpl()` queries, sequenced
  // to avoid the performance cost of concurrent calls.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})};

  base::WeakPtrFactory<BatteryLevelProviderWin> weak_ptr_factory_{this};
};

std::unique_ptr<BatteryLevelProvider> BatteryLevelProvider::Create() {
  return std::make_unique<BatteryLevelProviderWin>();
}

// static
absl::optional<BatteryLevelProvider::BatteryState>
BatteryLevelProviderWin::GetBatteryStateImpl() {
  // Proactively mark as blocking to fail early, since calls below may also
  // trigger ScopedBlockingCall.
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Battery interfaces are enumerated at every sample to detect when a new
  // interface is added, and avoid holding dangling handles when a battery is
  // disconnected.
  base::win::ScopedDevInfo devices(::SetupDiGetClassDevs(
      &GUID_DEVICE_BATTERY, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
  if (!devices.is_valid()) {
    return absl::nullopt;
  }

  std::vector<BatteryDetails> battery_details_list;

  // The algorithm to enumerate battery devices is taken from
  // https://docs.microsoft.com/en-us/windows/win32/power/enumerating-battery-devices
  // Limit search to 8 batteries max. A system may have several battery slots
  // and each slot may hold an actual battery.
  for (DWORD device_index = 0; device_index < 8; ++device_index) {
    SP_DEVICE_INTERFACE_DATA interface_data = {};
    interface_data.cbSize = sizeof(interface_data);

    BOOL success =
        ::SetupDiEnumDeviceInterfaces(devices.get(), 0, &GUID_DEVCLASS_BATTERY,
                                      device_index, &interface_data);
    if (!success) {
      // Enumeration ended normally.
      if (::GetLastError() == ERROR_NO_MORE_ITEMS)
        break;
      // Error.
      return absl::nullopt;
    }

    base::win::ScopedHandle battery =
        GetBatteryHandle(devices.get(), &interface_data);
    if (!battery.IsValid())
      return absl::nullopt;

    absl::optional<ULONG> battery_tag = GetBatteryTag(battery.Get());
    if (!battery_tag.has_value()) {
      return absl::nullopt;
    } else if (battery_tag.value() == BATTERY_TAG_INVALID) {
      // No battery present in this interface.
      continue;
    }

    auto battery_information =
        GetBatteryInformation(battery.Get(), *battery_tag);
    if (!battery_information.has_value()) {
      return absl::nullopt;
    }

    auto battery_status = GetBatteryStatus(battery.Get(), *battery_tag);
    if (!battery_status.has_value()) {
      return absl::nullopt;
    }

    battery_details_list.push_back(BatteryDetails(
        {.is_external_power_connected =
             !!(battery_status->PowerState & BATTERY_POWER_ON_LINE),
         .current_capacity = battery_status->Capacity,
         .full_charged_capacity = battery_information->FullChargedCapacity,
         .charge_unit =
             ((battery_information->Capabilities & BATTERY_CAPACITY_RELATIVE)
                  ? BatteryLevelUnit::kRelative
                  : BatteryLevelUnit::kMWh)}));
  }

  return MakeBatteryState(battery_details_list);
}

}  // namespace base
