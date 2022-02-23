// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_LINUX_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_LINUX_KEY_NETWORK_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace enterprise_connectors {

// Implementation of the LinuxKeyNetworkDelegate interface using mojo.
// This class does not support support parallel requests.
class LinuxKeyNetworkDelegate : public KeyNetworkDelegate {
 public:
  explicit LinuxKeyNetworkDelegate(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          remote_url_loader_factory);

  ~LinuxKeyNetworkDelegate() override;

  // KeyNetworkDelegate:
  KeyNetworkDelegate::HttpResponseCode SendPublicKeyToDmServerSync(
      const GURL& url,
      const std::string& dm_token,
      const std::string& body) override;

 private:
  // Starts the url upload key request using mojo's simple url loader.
  // `url` is the dm server url that the request is being sent to.
  // `dm_token` is the token given to the device during device enrollment.
  // `body` is the public key that is being sent.The `callback` represents
  // the callback that sets the HTTP status code.
  void StartRequest(base::OnceCallback<void(int)> callback,
                    const GURL& url,
                    const std::string& dm_token,
                    const std::string& body);

  // Invoked when the network url loader has completed.
  // `headers` represent the HTTP response headers received
  // from the server. The `callback` represents the callback
  // that sets the HTTP status code.
  void OnURLLoaderComplete(base::OnceCallback<void(int)> callback,
                           scoped_refptr<net::HttpResponseHeaders> headers);

  // Sets the HTTP status code for the response.
  // `response_code` represents the HTTP status code.
  void SetResponseCode(int response_code);

  // Used to capture the `response_code` received via SetResponseCode.
  int response_code_ = 0;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;
  base::WeakPtrFactory<LinuxKeyNetworkDelegate> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_LINUX_KEY_NETWORK_DELEGATE_H_
