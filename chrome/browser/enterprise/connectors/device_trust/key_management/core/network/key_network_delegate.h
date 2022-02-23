// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_H_

#include <string>

class GURL;

namespace enterprise_connectors {

// A delegate class that handles persistence of the key pair.  There is an
// implementation for each platform and also for tests.
class KeyNetworkDelegate {
 public:
  using HttpResponseCode = int;

  virtual ~KeyNetworkDelegate() = default;

  // Sends `body`, which is a serialized DeviceManagementRequest, to DM
  // server at `url`.  `dm_token` authn the local machine.  Only the
  // BrowserPublicKeyUploadRequest member is expected to be initialized.
  //
  // The return value is the upload response's HTTP status code.
  virtual HttpResponseCode SendPublicKeyToDmServerSync(
      const GURL& url,
      const std::string& dm_token,
      const std::string& body) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_H_
