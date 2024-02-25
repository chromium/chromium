// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_NETWORK_ADAPTER_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_NETWORK_ADAPTER_H_

#include <windows.networking.connectivity.h>
#include <wrl.h>

#include "base/memory/weak_ptr.h"

namespace media_router {
class FakeWinrtNetworkEnvironment;

class FakeNetworkAdapter final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt>,
          ABI::Windows::Networking::Connectivity::INetworkAdapter> {
 public:
  FakeNetworkAdapter(
      base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment,
      const GUID& network_adapter_id);

  // Implement the INetworkAdapter interface.
  HRESULT __stdcall get_NetworkAdapterId(GUID* value) final;
  // The following are not implemented because they are not used:
  HRESULT __stdcall get_OutboundMaxBitsPerSecond(UINT64* value) final;
  HRESULT __stdcall get_InboundMaxBitsPerSecond(UINT64* value) final;
  HRESULT __stdcall get_IanaInterfaceType(UINT32* value) final;
  HRESULT __stdcall get_NetworkItem(
      ABI::Windows::Networking::Connectivity::INetworkItem** value) final;
  HRESULT __stdcall GetConnectedProfileAsync(
      ABI::Windows::Foundation::IAsyncOperation<
          ABI::Windows::Networking::Connectivity::ConnectionProfile*>** value)
      final;

  FakeNetworkAdapter(const FakeNetworkAdapter&) = delete;
  FakeNetworkAdapter& operator=(const FakeNetworkAdapter&) = delete;

 private:
  ~FakeNetworkAdapter() final;

  base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment_;
  GUID network_adapter_id_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_NETWORK_ADAPTER_H_
