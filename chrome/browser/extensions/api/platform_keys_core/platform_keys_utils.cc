// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys_core/platform_keys_utils.h"

namespace extensions::platform_keys {

namespace {

constexpr char kTokenIdUser[] = "user";
constexpr char kTokenIdSystem[] = "system";

}  // anonymous namespace

// Returns a known token if |token_id| is valid and returns nullopt for both
// empty or unknown |token_id|.
std::optional<chromeos::platform_keys::TokenId> ApiIdToPlatformKeysTokenId(
    const std::string& token_id) {
  if (token_id == kTokenIdUser) {
    return chromeos::platform_keys::TokenId::kUser;
  }

  if (token_id == kTokenIdSystem) {
    return chromeos::platform_keys::TokenId::kSystem;
  }

  return std::nullopt;
}

}  // namespace extensions::platform_keys
