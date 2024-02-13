// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/test_support/win/fake_connection_profile.h"

#include "base/notimplemented.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_network_adapter.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_winrt_network_environment.h"
#include "chrome/browser/media/router/discovery/test_support/win/fake_wlan_connection_profile_details.h"

namespace WinrtConnectivity = ABI::Windows::Networking::Connectivity;
namespace WinrtFoundation = ABI::Windows::Foundation;

using Microsoft::WRL::Make;

namespace media_router {

FakeConnectionProfile::FakeConnectionProfile(
    base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment,
    const GUID& network_adapter_id,
    WinrtConnectivity::NetworkConnectivityLevel connectivity_level,
    const std::optional<std::string>& ssid)
    : fake_network_environment_(fake_network_environment),
      network_adapter_(Make<FakeNetworkAdapter>(fake_network_environment,
                                                network_adapter_id)),
      connectivity_level_(connectivity_level) {
  if (ssid) {
    wlan_connection_profile_details_ =
        Make<FakeWlanConnectionProfileDetails>(fake_network_environment, *ssid);
  }
}

HRESULT FakeConnectionProfile::QueryInterface(const IID& interface_id,
                                              void** result) {
  if (fake_network_environment_->GetErrorStatus() ==
      FakeWinrtNetworkStatus::kErrorConnectionProfileQueryInterfaceFailed) {
    return fake_network_environment_->MakeHresult(
        FakeWinrtNetworkStatus::kErrorConnectionProfileQueryInterfaceFailed);
  }

  // Call the base class implementation.
  return Microsoft::WRL::RuntimeClass<
      Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt>,
      WinrtConnectivity::IConnectionProfile,
      WinrtConnectivity::IConnectionProfile2>::QueryInterface(interface_id,
                                                              result);
}

HRESULT FakeConnectionProfile::GetNetworkConnectivityLevel(
    WinrtConnectivity::NetworkConnectivityLevel* value) {
  if (fake_network_environment_->GetErrorStatus() ==
      FakeWinrtNetworkStatus::
          kErrorConnectionProfileGetNetworkConnectivityLevelFailed) {
    return fake_network_environment_->MakeHresult(
        FakeWinrtNetworkStatus::
            kErrorConnectionProfileGetNetworkConnectivityLevelFailed);
  }

  *value = connectivity_level_;
  return S_OK;
}

HRESULT FakeConnectionProfile::get_ProfileName(HSTRING* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::get_NetworkAdapter(
    WinrtConnectivity::INetworkAdapter** value) {
  if (fake_network_environment_->GetErrorStatus() ==
      FakeWinrtNetworkStatus::kErrorConnectionProfileGetNetworkAdapterFailed) {
    return fake_network_environment_->MakeHresult(
        FakeWinrtNetworkStatus::kErrorConnectionProfileGetNetworkAdapterFailed);
  }

  return network_adapter_.CopyTo(value);
}

HRESULT FakeConnectionProfile::get_WlanConnectionProfileDetails(
    WinrtConnectivity::IWlanConnectionProfileDetails** value) {
  if (fake_network_environment_->GetErrorStatus() ==
      FakeWinrtNetworkStatus::
          kErrorConnectionProfileGetWlanConnectionProfileDetailsFailed) {
    return fake_network_environment_->MakeHresult(
        FakeWinrtNetworkStatus::
            kErrorConnectionProfileGetWlanConnectionProfileDetailsFailed);
  }
  return wlan_connection_profile_details_.CopyTo(value);
}

HRESULT FakeConnectionProfile::GetNetworkNames(
    WinrtFoundation::Collections::IVectorView<HSTRING>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetConnectionCost(
    WinrtConnectivity::IConnectionCost** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetDataPlanStatus(
    WinrtConnectivity::IDataPlanStatus** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetLocalUsage(
    WinrtFoundation::DateTime StartTime,
    WinrtFoundation::DateTime EndTime,
    WinrtConnectivity::IDataUsage** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetLocalUsagePerRoamingStates(
    WinrtFoundation::DateTime StartTime,
    WinrtFoundation::DateTime EndTime,
    WinrtConnectivity::RoamingStates States,
    WinrtConnectivity::IDataUsage** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::get_NetworkSecuritySettings(
    WinrtConnectivity::INetworkSecuritySettings** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::get_IsWwanConnectionProfile(boolean* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::get_IsWlanConnectionProfile(boolean* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::get_WwanConnectionProfileDetails(
    WinrtConnectivity::IWwanConnectionProfileDetails** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::get_ServiceProviderGuid(
    WinrtFoundation::IReference<GUID>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetSignalBars(
    WinrtFoundation::IReference<byte>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetDomainConnectivityLevel(
    WinrtConnectivity::DomainConnectivityLevel* value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetNetworkUsageAsync(
    WinrtFoundation::DateTime startTime,
    WinrtFoundation::DateTime endTime,
    WinrtConnectivity::DataUsageGranularity granularity,
    WinrtConnectivity::NetworkUsageStates states,
    WinrtFoundation::IAsyncOperation<WinrtFoundation::Collections::IVectorView<
        WinrtConnectivity::NetworkUsage*>*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeConnectionProfile::GetConnectivityIntervalsAsync(
    WinrtFoundation::DateTime startTime,
    WinrtFoundation::DateTime endTime,
    WinrtConnectivity::NetworkUsageStates states,
    WinrtFoundation::IAsyncOperation<WinrtFoundation::Collections::IVectorView<
        WinrtConnectivity::ConnectivityInterval*>*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

FakeConnectionProfile::~FakeConnectionProfile() = default;

}  // namespace media_router
