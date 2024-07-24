// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_TOKEN_FETCHER_H_

#include <optional>

#include "base/functional/callback_forward.h"

namespace babelorca {

struct TokenDataWrapper;

// Interface for fetching tokens needed for authentications.
class TokenFetcher {
 public:
  // Callback executed on fetch response, no value means fetch failure.
  using TokenFetchCallback =
      base::OnceCallback<void(std::optional<TokenDataWrapper>)>;

  TokenFetcher(const TokenFetcher&) = delete;
  TokenFetcher& operator=(const TokenFetcher&) = delete;

  virtual ~TokenFetcher() = default;

  virtual void fetchToken(TokenFetchCallback callback) = 0;

 protected:
  TokenFetcher() = default;
};

}  // namespace babelorca

#endif  // CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_TOKEN_FETCHER_H_
