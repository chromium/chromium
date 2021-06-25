// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
namespace platform_keys {

const char kErrorInvalidToken[] = "The token is not valid.";

namespace {
const char kTokenIdUser[] = "user";
const char kTokenIdSystem[] = "system";
}  // namespace

absl::optional<chromeos::platform_keys::TokenId> ApiIdToPlatformKeysTokenId(
    const std::string& token_id) {
  if (token_id == kTokenIdUser)
    return chromeos::platform_keys::TokenId::kUser;

  if (token_id == kTokenIdSystem)
    return chromeos::platform_keys::TokenId::kSystem;

  return absl::nullopt;
}

}  // namespace platform_keys
}  // namespace extensions
