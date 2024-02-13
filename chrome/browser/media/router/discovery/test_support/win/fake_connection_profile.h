// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_CONNECTION_PROFILE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_CONNECTION_PROFILE_H_

#include <windows.networking.connectivity.h>
#include <wrl.h>

#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"

namespace media_router {
class FakeWinrtNetworkEnvironment;

class FakeConnectionProfile final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt>,
          ABI::Windows::Networking::Connectivity::IConnectionProfile,
          ABI::Windows::Networking::Connectivity::IConnectionProfile2> {
 public:
  FakeConnectionProfile(
      base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment,
      const GUID& network_adapter_id,
      ABI::Windows::Networking::Connectivity::NetworkConnectivityLevel
          connectivity_level,
      const std::optional<std::string>& ssid);

  // Override the IUnknown interface to simulate failures.
  HRESULT __stdcall QueryInterface(const IID& interface_id,
                                   void** result) final;

  // Implement the IConnectionProfile & IConnectionProfile2 interfaces.
  HRESULT __stdcall GetNetworkConnectivityLevel(
      ABI::Windows::Networking::Connectivity::NetworkConnectivityLevel* value)
      final;
  HRESULT __stdcall get_ProfileName(HSTRING* value) final;
  HRESULT __stdcall get_NetworkAdapter(
      ABI::Windows::Networking::Connectivity::INetworkAdapter** value) final;
  HRESULT __stdcall get_IsWlanConnectionProfile(boolean* value) final;
  // The following are not implemented because they are not used:
  HRESULT __stdcall GetNetworkNames(
      ABI::Windows::Foundation::Collections::IVectorView<HSTRING>** value)
      final;
  HRESULT __stdcall GetConnectionCost(
      ABI::Windows::Networking::Connectivity::IConnectionCost** value) final;
  HRESULT __stdcall GetDataPlanStatus(
      ABI::Windows::Networking::Connectivity::IDataPlanStatus** value) final;
  HRESULT __stdcall GetLocalUsage(
      ABI::Windows::Foundation::DateTime StartTime,
      ABI::Windows::Foundation::DateTime EndTime,
      ABI::Windows::Networking::Connectivity::IDataUsage** value) final;
  HRESULT __stdcall GetLocalUsagePerRoamingStates(
      ABI::Windows::Foundation::DateTime StartTime,
      ABI::Windows::Foundation::DateTime EndTime,
      ABI::Windows::Networking::Connectivity::RoamingStates States,
      ABI::Windows::Networking::Connectivity::IDataUsage** value) final;
  HRESULT __stdcall get_NetworkSecuritySettings(
      ABI::Windows::Networking::Connectivity::INetworkSecuritySettings** value)
      final;
  HRESULT __stdcall get_IsWwanConnectionProfile(boolean* value) final;
  HRESULT __stdcall get_WwanConnectionProfileDetails(
      ABI::Windows::Networking::Connectivity::IWwanConnectionProfileDetails**
          value) final;
  HRESULT __stdcall get_WlanConnectionProfileDetails(
      ABI::Windows::Networking::Connectivity::IWlanConnectionProfileDetails**
          value) final;
  HRESULT __stdcall get_ServiceProviderGuid(
      ABI::Windows::Foundation::IReference<GUID>** value) final;
  HRESULT __stdcall GetSignalBars(
      ABI::Windows::Foundation::IReference<byte>** value) final;
  HRESULT __stdcall GetDomainConnectivityLevel(
      ABI::Windows::Networking::Connectivity::DomainConnectivityLevel* value)
      final;
  HRESULT __stdcall GetNetworkUsageAsync(
      ABI::Windows::Foundation::DateTime startTime,
      ABI::Windows::Foundation::DateTime endTime,
      ABI::Windows::Networking::Connectivity::DataUsageGranularity granularity,
      ABI::Windows::Networking::Connectivity::NetworkUsageStates states,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Foundation::Collections::IVectorView<
              ABI::Windows::Networking::Connectivity::NetworkUsage*>*>** value)
      final;
  HRESULT __stdcall GetConnectivityIntervalsAsync(
      ABI::Windows::Foundation::DateTime startTime,
      ABI::Windows::Foundation::DateTime endTime,
      ABI::Windows::Networking::Connectivity::NetworkUsageStates states,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Foundation::Collections::IVectorView<
              ABI::Windows::Networking::Connectivity::ConnectivityInterval*>*>**
          value) final;

  FakeConnectionProfile(const FakeConnectionProfile&) = delete;
  FakeConnectionProfile& operator=(const FakeConnectionProfile&) = delete;

 private:
  ~FakeConnectionProfile() final;

  base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Networking::Connectivity::INetworkAdapter>
      network_adapter_;

  ABI::Windows::Networking::Connectivity::NetworkConnectivityLevel
      connectivity_level_;

  Microsoft::WRL::ComPtr<
      ABI::Windows::Networking::Connectivity::IWlanConnectionProfileDetails>
      wlan_connection_profile_details_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_CONNECTION_PROFILE_H_
