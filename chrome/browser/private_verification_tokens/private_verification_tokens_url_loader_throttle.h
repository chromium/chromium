// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_URL_LOADER_THROTTLE_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace network {
class SharedURLLoaderFactory;
}

class PrivateVerificationTokensService;

// Trigger PVT issuance and add PVT header when needed.
class PrivateVerificationTokensURLLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  static std::unique_ptr<PrivateVerificationTokensURLLoaderThrottle> Create(
      PrivateVerificationTokensService* pvt_service,
      bool is_off_the_record,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~PrivateVerificationTokensURLLoaderThrottle() override;

  // blink::URLLoaderThrottle:
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      network::HttpRequestHeadersUpdateParams* headers_update_params) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

 private:
  PrivateVerificationTokensURLLoaderThrottle(
      base::WeakPtr<PrivateVerificationTokensService> pvt_service,
      bool is_off_the_record,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  base::WeakPtr<PrivateVerificationTokensService> pvt_service_;
  bool is_off_the_record_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::optional<int64_t> token_id_;
};

#endif  // CHROME_BROWSER_PRIVATE_VERIFICATION_TOKENS_PRIVATE_VERIFICATION_TOKENS_URL_LOADER_THROTTLE_H_
