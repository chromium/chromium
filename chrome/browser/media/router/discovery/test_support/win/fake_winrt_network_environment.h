// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_WINRT_NETWORK_ENVIRONMENT_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_WINRT_NETWORK_ENVIRONMENT_H_

#include <roapi.h>
#include <windows.networking.connectivity.h>
#include <wrl/client.h>

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"

namespace media_router {

// Each value represents a different WinRT API that can fail during
// GetDiscoveryNetworkInfoList().  Use with
// `FakeWinrtNetworkEnvironment::SimulateError` to simulate WinRT API failures
// that return error HRESULTS.
enum class FakeWinrtNetworkStatus {
  kOk = 0,
  kErrorRoGetActivationFactoryFailed = 1,
  kErrorNetworkInformationStaticsGetConnectionProfilesFailed = 2,
  kErrorVectorViewGetAtFailed = 3,
  kErrorVectorViewGetSizeFailed = 4,
  kErrorConnectionProfileQueryInterfaceFailed = 5,
  kErrorConnectionProfileGetNetworkConnectivityLevelFailed = 6,
  kErrorConnectionProfileGetNetworkAdapterFailed = 7,
  kErrorConnectionProfileGetWlanConnectionProfileDetailsFailed = 8,
  kErrorWlanConnectionProfileDetailsGetConnectedSsidFailed = 9,
  kErrorGetNetworkAdapterIdFailed = 10,
};

// Provides a fake implementation of RoGetActivationFactory() to creates a fake
// implementation of INetworkInformationStatics, which is used to enumerate fake
// connection profiles.  Tests should use this class to simulate different
// network environments with different types of adapters and different levels
// of connectivity.
class FakeWinrtNetworkEnvironment final {
 public:
  FakeWinrtNetworkEnvironment();
  ~FakeWinrtNetworkEnvironment();

  FakeWinrtNetworkStatus GetErrorStatus() const;
  void SimulateError(FakeWinrtNetworkStatus error_status);

  // Gets the fake HRESULT that the fake OS API implementation will return for
  // `error_status`.
  HRESULT MakeHresult(FakeWinrtNetworkStatus error_status) const;

  // Sets up the fake network environment by creating fake network adapters. Use
  // `std::nullopt` for `ssid` to create a wired network adapter.  Stores
  // fake network adapters in the `connection_profiles_` member.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Networking::Connectivity::IConnectionProfile>
  AddConnectionProfile(
      const GUID& network_adapter_id,
      ABI::Windows::Networking::Connectivity::NetworkConnectivityLevel
          connectivity_level,
      const std::optional<std::string>& ssid);

  const std::vector<Microsoft::WRL::ComPtr<
      ABI::Windows::Networking::Connectivity::IConnectionProfile>>&
  GetConnectionProfiles() const;

  HRESULT FakeRoGetActivationFactory(HSTRING class_id,
                                     const IID& iid,
                                     void** factory);

  FakeWinrtNetworkEnvironment(const FakeWinrtNetworkEnvironment&) = delete;
  FakeWinrtNetworkEnvironment& operator=(const FakeWinrtNetworkEnvironment&) =
      delete;

 private:
  // Specifies which error the fake WinRT network environment should simulate.
  FakeWinrtNetworkStatus error_status_ = FakeWinrtNetworkStatus::kOk;

  // Contains the fake network adapters enumerated by the WinRT API.
  std::vector<Microsoft::WRL::ComPtr<
      ABI::Windows::Networking::Connectivity::IConnectionProfile>>
      connection_profiles_;

  base::WeakPtrFactory<FakeWinrtNetworkEnvironment> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_WINRT_NETWORK_ENVIRONMENT_H_
