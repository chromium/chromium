// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_H_

#include <string>

#include "base/functional/callback.h"

class GURL;

namespace enterprise_connectors {

// A delegate class that handles persistence of the key pair. There is an
// implementation for each platform and also for tests.
class KeyNetworkDelegate {
 public:
  using HttpResponseCode = int;

  // Upload key completion callback. The single argument is the response code
  // of the network request or 0 if the response code text exists but could
  // not be parsed.
  using UploadKeyCompletedCallback = base::OnceCallback<void(HttpResponseCode)>;

  virtual ~KeyNetworkDelegate() = default;

  // Sends `body`, which is a serialized DeviceManagementRequest, to DM
  // server at `url`. `dm_token` authn the local machine. Only the
  // BrowserPublicKeyUploadRequest member is expected to be initialized.
  // The HTTP response of the upload request is returned using the
  // `key_upload_completed_callback`.
  //
  // Only a single call to SendPublicKeyToDmServer is expected during the
  // key rotation.
  virtual void SendPublicKeyToDmServer(
      const GURL& url,
      const std::string& dm_token,
      const std::string& body,
      UploadKeyCompletedCallback key_upload_completed_callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_H_
