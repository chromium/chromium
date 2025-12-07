// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys_core/platform_keys_utils.h"

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

namespace extensions::platform_keys {

namespace {

constexpr char kTokenIdUser[] = "user";
constexpr char kTokenIdSystem[] = "system";

}  // anonymous namespace

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

bool IsExtensionAllowed(Profile* profile, const Extension* extension) {
  if (Manifest::IsComponentLocation(extension->location())) {
    // Note: For this to even be called, the component extension must also be
    // allowed in chrome/common/extensions/api/_permission_features.json
    return true;
  }
  const base::Value::List& list =
      profile->GetPrefs()->GetList(prefs::kAttestationExtensionAllowlist);
  base::Value value(extension->id());
  return base::Contains(list, value);
}

}  // namespace extensions::platform_keys
