// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_MOJO_KEY_NETWORK_DELEGATE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_MOJO_KEY_NETWORK_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace enterprise_connectors {

// Implementation of the MojoKeyNetworkDelegate interface using mojo.
// This class does not support support parallel requests.
class MojoKeyNetworkDelegate : public KeyNetworkDelegate,
                               public client_certificates::DMServerClient {
 public:
  explicit MojoKeyNetworkDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  ~MojoKeyNetworkDelegate() override;

  // KeyNetworkDelegate:
  void SendPublicKeyToDmServer(
      const GURL& url,
      const std::string& dm_token,
      const std::string& body,
      UploadKeyCompletedCallback upload_key_completed_callback) override;

  // client_certificates::DMServerClient:
  void SendRequest(
      const GURL& url,
      std::string_view dm_token,
      const enterprise_management::DeviceManagementRequest& request_body,
      SendRequestCallback callback) override;

 private:
  // Invoked when the network url loader has completed. `headers` is
  // the HTTP response headers received from the server.
  // `upload_key_completed_callback` is used to return the HTTP status
  // code.
  void OnURLLoaderComplete(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      UploadKeyCompletedCallback upload_key_completed_callback,
      scoped_refptr<net::HttpResponseHeaders> headers);

  void OnDownloadStringComplete(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      SendRequestCallback callback,
      std::optional<std::string> response_body);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  base::WeakPtrFactory<MojoKeyNetworkDelegate> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_MOJO_KEY_NETWORK_DELEGATE_H_
