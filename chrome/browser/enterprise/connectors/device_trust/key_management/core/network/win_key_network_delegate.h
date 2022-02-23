// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

// Windows implementation of the KeyNetworkDelegate interface.
class WinKeyNetworkDelegate : public KeyNetworkDelegate {
 public:
  WinKeyNetworkDelegate();
  ~WinKeyNetworkDelegate() override;

  // KeyNetworkDelegate:
  HttpResponseCode SendPublicKeyToDmServerSync(
      const GURL& url,
      const std::string& dm_token,
      const std::string& body) override;

 private:
  // Invoked when the network fetch has completed. `response_code` represents
  // the HTTP status code for the response.
  void FetchCompleted(int response_code);

  // Used to capture the `response_code` received via FetchCompleted.
  absl::optional<int> response_code_;

  base::WeakPtrFactory<WinKeyNetworkDelegate> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
