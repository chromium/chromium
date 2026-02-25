// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_ALPHA_RPC_H_
#define CHROME_BROWSER_INDIGO_INDIGO_ALPHA_RPC_H_

#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace indigo {

struct AlphaGenerateError {
  int error_type;
  std::string error_message;
};

// Parse the response from a request to the server endpoint used in early
// development. This should not be used in production.
base::expected<GURL, AlphaGenerateError> ParseAlphaGenerateResponse(
    std::string_view body);
base::expected<void, std::string> ParseAlphaStatusResponse(
    std::string_view body);

// Executes the alpha generate RPC and calls the callback with the parsed
// response.
void ExecuteAlphaGenerateRpc(
    network::SharedURLLoaderFactory* loader_factory,
    base::OnceCallback<void(base::expected<GURL, AlphaGenerateError>)>
        callback);

// Executes the alpha status RPC and calls the callback with the parsed
// response.
void ExecuteAlphaStatusRpc(
    network::SharedURLLoaderFactory* loader_factory,
    base::OnceCallback<void(base::expected<void, std::string>)> callback);

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_ALPHA_RPC_H_
