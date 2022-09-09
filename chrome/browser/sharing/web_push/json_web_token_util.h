// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_WEB_PUSH_JSON_WEB_TOKEN_UTIL_H_
#define CHROME_BROWSER_SHARING_WEB_PUSH_JSON_WEB_TOKEN_UTIL_H_

#include <string>

#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crypto {
class ECPrivateKey;
}

// Creates JSON web token with provided |payload|, and sign  with provided
// |private_key|, as per RFC7519.
// |claims|: A Value of DICTIONARY type containing claims between two parties.
// |private_key|: An elliptic curve (EC) private key.
// Note: Currently only ES256 is supported, as ECPrivateKey only supports
// NIST P-256 curve and ECSignatureCreator is hardcoded to SHA256.
//
// https://tools.ietf.org/html/rfc7519
absl::optional<std::string> CreateJSONWebToken(
    const base::Value& claims,
    crypto::ECPrivateKey* private_key);

#endif  // CHROME_BROWSER_SHARING_WEB_PUSH_JSON_WEB_TOKEN_UTIL_H_
