// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/test_support/win/fake_winrt_network_environment.h"

#include <tuple>

#include "base/notreached.h"
#include "base/win/scoped_hstring.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_connection_profile.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_network_information_statics.h"

namespace WinrtConnectivity = ABI::Windows::Networking::Connectivity;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;

namespace media_router {

FakeWinrtNetworkEnvironment::FakeWinrtNetworkEnvironment() = default;

FakeWinrtNetworkEnvironment::~FakeWinrtNetworkEnvironment() = default;

FakeWinrtNetworkStatus FakeWinrtNetworkEnvironment::GetErrorStatus() const {
  return error_status_;
}

HRESULT FakeWinrtNetworkEnvironment::MakeHresult(
    FakeWinrtNetworkStatus error_status) const {
  return MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF,
                      /*base=*/0x800 + static_cast<int>(error_status));
}

void FakeWinrtNetworkEnvironment::SimulateError(
    FakeWinrtNetworkStatus error_status) {
  error_status_ = error_status;
}

const std::vector<ComPtr<WinrtConnectivity::IConnectionProfile>>&
FakeWinrtNetworkEnvironment::GetConnectionProfiles() const {
  return connection_profiles_;
}

ComPtr<WinrtConnectivity::IConnectionProfile>
FakeWinrtNetworkEnvironment::AddConnectionProfile(
    const GUID& network_adapter_id,
    WinrtConnectivity::NetworkConnectivityLevel connectivity_level,
    const std::optional<std::string>& ssid) {
  ComPtr<WinrtConnectivity::IConnectionProfile> new_connection_profile =
      Make<FakeConnectionProfile>(weak_ptr_factory_.GetWeakPtr(),
                                  network_adapter_id, connectivity_level, ssid);

  connection_profiles_.push_back(new_connection_profile);
  return new_connection_profile;
}

HRESULT FakeWinrtNetworkEnvironment::FakeRoGetActivationFactory(
    HSTRING class_id,
    const IID& iid,
    void** factory) {
  if (error_status_ ==
      FakeWinrtNetworkStatus::kErrorRoGetActivationFactoryFailed) {
    return MakeHresult(
        FakeWinrtNetworkStatus::kErrorRoGetActivationFactoryFailed);
  }

  base::win::ScopedHString class_id_hstring(class_id);

  if (class_id_hstring.Get() ==
      RuntimeClass_Windows_Networking_Connectivity_NetworkInformation) {
    ComPtr<FakeNetworkInformationStatics> network_info_statics =
        Make<FakeNetworkInformationStatics>(weak_ptr_factory_.GetWeakPtr());
    *factory = network_info_statics.Detach();
  }
  std::ignore = class_id_hstring.release();

  if (*factory == nullptr) {
    NOTIMPLEMENTED();
    return E_NOTIMPL;
  }
  return S_OK;
}

}  // namespace media_router
