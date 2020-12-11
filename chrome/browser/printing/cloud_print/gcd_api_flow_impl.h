// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_IMPL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/cloud_print/gcd_api_flow.h"
#include "components/signin/public/identity_manager/access_token_info.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}
namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network
class GoogleServiceAuthError;

namespace cloud_print {

class GCDApiFlowImpl : public GCDApiFlow {
 public:
  // Create an OAuth2-based confirmation.
  GCDApiFlowImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~GCDApiFlowImpl() override;

  // GCDApiFlow implementation:
  void Start(std::unique_ptr<Request> request) override;

  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo access_token_info);

 private:
  void OnDownloadedToString(std::unique_ptr<std::string> response_body);

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  signin::IdentityManager* identity_manager_;
  std::unique_ptr<Request> request_;
  base::WeakPtrFactory<GCDApiFlowImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GCDApiFlowImpl);
};

}  // namespace cloud_print

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_GCD_API_FLOW_IMPL_H_
