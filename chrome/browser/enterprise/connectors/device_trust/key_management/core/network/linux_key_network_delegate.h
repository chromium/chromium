// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_LINUX_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_LINUX_KEY_NETWORK_DELEGATE_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

#include <memory>
#include <string>

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
  void SendPublicKeyToDmServer(
      const GURL& url,
      const std::string& dm_token,
      const std::string& body,
      UploadKeyCompletedCallback upload_key_completed_callback) override;

 private:
  // Invoked when the network url loader has completed. `headers` is
  // the HTTP response headers received from the server.
  // `upload_key_completed_callback` is used to return the HTTP status
  // code.
  void OnURLLoaderComplete(
      UploadKeyCompletedCallback upload_key_completed_callback,
      scoped_refptr<net::HttpResponseHeaders> headers);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;
  base::WeakPtrFactory<LinuxKeyNetworkDelegate> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_LINUX_KEY_NETWORK_DELEGATE_H_
