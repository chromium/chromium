// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_WLAN_CONNECTION_PROFILE_DETAILS_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_WLAN_CONNECTION_PROFILE_DETAILS_H_

#include <windows.networking.connectivity.h>
#include <wrl.h>

#include <string>

#include "base/memory/weak_ptr.h"

namespace media_router {
class FakeWinrtNetworkEnvironment;

class FakeWlanConnectionProfileDetails final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRt>,
          ABI::Windows::Networking::Connectivity::
              IWlanConnectionProfileDetails> {
 public:
  FakeWlanConnectionProfileDetails(
      base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment,
      const std::string& ssid);

  // Implement the IWlanConnectionProfileDetails interface.
  HRESULT __stdcall GetConnectedSsid(HSTRING* value) final;

  FakeWlanConnectionProfileDetails(const FakeWlanConnectionProfileDetails&) =
      delete;
  FakeWlanConnectionProfileDetails& operator=(
      const FakeWlanConnectionProfileDetails&) = delete;

 private:
  ~FakeWlanConnectionProfileDetails() final;

  base::WeakPtr<FakeWinrtNetworkEnvironment> fake_network_environment_;
  std::string ssid_;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_TEST_SUPPORT_WIN_FAKE_WLAN_CONNECTION_PROFILE_DETAILS_H_
