// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "components/winhttp/network_fetcher.h"
#include "components/winhttp/scoped_hinternet.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

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
  friend class WinKeyNetworkDelegateTest;

  // Callback to the upload key function. The `callback` sets the HTTP
  // status code. `url` is the dm server url that the request is being
  // sent to.  `dm_token` is the token given to the device during
  // device enrollment, and `body` is the public key that is being sent.
  using UploadKeyCallback = base::RepeatingCallback<void(
      base::OnceCallback<void(int)> callback,
      const base::flat_map<std::string, std::string>& headers,
      const GURL& url,
      const std::string& body)>;

  // Strictly used for testing and allows mocking the upload key process.
  // The `upload_callback` is a callback to the UploadKey function.
  // `sleep_during_backoff` is set to false for testing and is used to keep
  // the test from timing out.
  WinKeyNetworkDelegate(UploadKeyCallback upload_callback,
                        bool sleep_during_backoff);

  void UploadKey(base::OnceCallback<void(int)> callback,
                 const base::flat_map<std::string, std::string>& headers,
                 const GURL& url,
                 const std::string& body);

  // Invoked when the network fetch has completed. `response_code` represents
  // the HTTP status code for the response.
  void FetchCompleted(int response_code);

  // Used to capture the `response_code` received via FetchCompleted.
  absl::optional<int> response_code_;

  UploadKeyCallback upload_callback_;

  winhttp::ScopedHInternet winhttp_session_;
  scoped_refptr<winhttp::NetworkFetcher> winhttp_network_fetcher_;

  // Used to bound whether the exponential backoff should sleep.
  // This is only set to false when testing.
  const bool sleep_during_backoff_;

  base::WeakPtrFactory<WinKeyNetworkDelegate> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_WIN_KEY_NETWORK_DELEGATE_H_
