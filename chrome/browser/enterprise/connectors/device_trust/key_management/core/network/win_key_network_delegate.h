// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

namespace enterprise_connectors {

// Windows implementation of the KeyNetworkDelegate interface.
class WinKeyNetworkDelegate : public KeyNetworkDelegate {
 public:
  WinKeyNetworkDelegate();
  ~WinKeyNetworkDelegate() override;

  // KeyNetworkDelegate:
  std::string SendPublicKeyToDmServerSync(const GURL& url,
                                          const std::string& dm_token,
                                          const std::string& body) override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
