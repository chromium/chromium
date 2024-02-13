// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_NETWORK_INFORMATION_STATICS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_NETWORK_INFORMATION_STATICS_H_

#include <windows.networking.connectivity.h>
#include <wrl.h>

#include "base/memory/weak_ptr.h"

namespace media_router {
class FakeWinrtNetworkEnvironment;

class FakeNetworkInformationStatics final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt>,
          ABI::Windows::Networking::Connectivity::INetworkInformationStatics> {
 public:
  FakeNetworkInformationStatics(
      base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment);

  // Implement the INetworkInformationStatics interface.
  HRESULT __stdcall GetConnectionProfiles(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Networking::Connectivity::ConnectionProfile*>** value)
      final;
  // The following are not implemented because they are not used:
  HRESULT __stdcall GetInternetConnectionProfile(
      ABI::Windows::Networking::Connectivity::IConnectionProfile** value) final;
  HRESULT __stdcall GetLanIdentifiers(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Networking::Connectivity::LanIdentifier*>** value)
      final;
  HRESULT __stdcall GetHostNames(
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Networking::HostName*>** value) final;
  HRESULT __stdcall GetProxyConfigurationAsync(
      ABI::Windows::Foundation::IUriRuntimeClass* uri,
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Networking::Connectivity::ProxyConfiguration*>** value)
      final;
  HRESULT __stdcall GetSortedEndpointPairs(
      ABI::Windows::Foundation::Collections::IIterable<
          ABI::Windows::Networking::EndpointPair*>* destination_list,
      ABI::Windows::Networking::HostNameSortOptions sort_options,
      ABI::Windows::Foundation::Collections::IVectorView<
          ABI::Windows::Networking::EndpointPair*>** value) final;
  HRESULT __stdcall add_NetworkStatusChanged(
      ABI::Windows::Networking::Connectivity::INetworkStatusChangedEventHandler*
          network_status_handler,
      EventRegistrationToken* event_cookie) final;
  HRESULT __stdcall remove_NetworkStatusChanged(
      EventRegistrationToken event_cookie) final;

  FakeNetworkInformationStatics(const FakeNetworkInformationStatics&) = delete;
  FakeNetworkInformationStatics& operator=(
      const FakeNetworkInformationStatics&) = delete;

 private:
  ~FakeNetworkInformationStatics() final;

  base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_NETWORK_INFORMATION_STATICS_H_
