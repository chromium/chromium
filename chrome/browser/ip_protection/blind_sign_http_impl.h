// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IP_PROTECTION_BLIND_SIGN_HTTP_IMPL_H_
#define CHROME_BROWSER_IP_PROTECTION_BLIND_SIGN_HTTP_IMPL_H_

#include <functional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_http_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {
class SimpleURLLoader;
}  // namespace network
class BlindSignHttpImpl : public quiche::BlindSignHttpInterface {
 public:
  explicit BlindSignHttpImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BlindSignHttpImpl() override;
  void DoRequest(
      const std::string& path_and_query,
      const std::string& authorization_header,
      const std::string& body,
      std::function<void(absl::StatusOr<quiche::BlindSignHttpResponse>)>
          callback) override;

 private:
  void OnRequestCompleted(std::unique_ptr<std::string> response);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::function<void(absl::StatusOr<quiche::BlindSignHttpResponse>)> callback_;

  base::WeakPtrFactory<BlindSignHttpImpl> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_IP_PROTECTION_BLIND_SIGN_HTTP_IMPL_H_
