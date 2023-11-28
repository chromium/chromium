// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_HTTP_H_
#define CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_HTTP_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_http_interface.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace ip_protection {
class GetProxyConfigResponse;
}

// HTTP Fetching for IP Protection. This implements the `BlindSignHttpInterface`
// for use by the BSA library, and also provides methods used directly by
// `IpProtectionConfigProvider`.
class IpProtectionConfigHttp : public quiche::BlindSignHttpInterface {
 public:
  explicit IpProtectionConfigHttp(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~IpProtectionConfigHttp() override;

  // BlindSignHttp implementation.
  void DoRequest(quiche::BlindSignHttpRequestType request_type,
                 const std::string& authorization_header,
                 const std::string& body,
                 quiche::BlindSignHttpCallback callback) override;

  using GetProxyConfigCallback = base::OnceCallback<void(
      absl::StatusOr<ip_protection::GetProxyConfigResponse>)>;
  virtual void GetProxyConfig(GetProxyConfigCallback callback,
                              bool for_testing = false);

 private:
  void OnDoRequestCompleted(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      quiche::BlindSignHttpCallback callback,
      std::unique_ptr<std::string> response);
  void OnGetProxyConfigCompleted(
      std::unique_ptr<network::SimpleURLLoader> url_loader,
      GetProxyConfigCallback callback,
      std::unique_ptr<std::string> response);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  const GURL ip_protection_server_url_;
  const std::string ip_protection_server_get_initial_data_path_;
  const std::string ip_protection_server_get_tokens_path_;
  const std::string ip_protection_server_get_proxy_config_path_;

  base::WeakPtrFactory<IpProtectionConfigHttp> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_IP_PROTECTION_IP_PROTECTION_CONFIG_HTTP_H_
