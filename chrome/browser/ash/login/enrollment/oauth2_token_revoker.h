// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_OAUTH2_TOKEN_REVOKER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_OAUTH2_TOKEN_REVOKER_H_

#include "base/memory/scoped_refptr.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class OAuth2TokenRevoker {
 public:
  // `shared_url_loader_factory` must be non-null.
  OAuth2TokenRevoker(
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  OAuth2TokenRevoker(const OAuth2TokenRevoker&) = delete;
  OAuth2TokenRevoker& operator=(const OAuth2TokenRevoker&) = delete;
  ~OAuth2TokenRevoker();

  void Start(const std::string& token);

 private:
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_OAUTH2_TOKEN_REVOKER_H_
