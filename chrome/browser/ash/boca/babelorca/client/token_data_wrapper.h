// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_TOKEN_DATA_WRAPPER_H_
#define CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_TOKEN_DATA_WRAPPER_H_

#include <string>
#include <utility>

#include "base/time/time.h"

namespace babelorca {

struct TokenDataWrapper {
  std::string token;
  base::Time expiration_time;

  TokenDataWrapper() = default;
  TokenDataWrapper(std::string token_param, base::Time expiration_time_param)
      : token(std::move(token_param)), expiration_time(expiration_time_param) {}
};

}  // namespace babelorca

#endif  // CHROME_BROWSER_ASH_BOCA_BABELORCA_CLIENT_TOKEN_DATA_WRAPPER_H_
