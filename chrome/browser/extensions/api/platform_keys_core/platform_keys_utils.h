// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_CORE_PLATFORM_KEYS_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_CORE_PLATFORM_KEYS_UTILS_H_

#include <optional>
#include <string>

#include "chrome/browser/chromeos/platform_keys/platform_keys.h"

class Extension;
class Profile;

namespace extensions::platform_keys {

inline constexpr char kErrorInvalidToken[] = "The token is not valid.";
inline constexpr char kErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";

// Returns a known token if `token_id` is valid and returns nullopt for both
// empty or invalid values of `token_id`.
std::optional<chromeos::platform_keys::TokenId> ApiIdToPlatformKeysTokenId(
    const std::string& token_id);

bool IsExtensionAllowed(Profile* profile, const Extension* extension);

}  // namespace extensions::platform_keys

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_CORE_PLATFORM_KEYS_UTILS_H_
