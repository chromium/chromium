// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/cookie_controls_mode_transformer.h"

#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using enum content_settings::CookieControlsMode;

CookieControlsModeTransformer::CookieControlsModeTransformer() = default;
CookieControlsModeTransformer::~CookieControlsModeTransformer() = default;

std::optional<base::Value>
CookieControlsModeTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  bool third_party_cookies_allowed = extension_pref.GetBool();
  return base::Value(
      static_cast<int>(third_party_cookies_allowed ? kOff : kBlockThirdParty));
}

std::optional<base::Value>
CookieControlsModeTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  auto cookie_control_mode =
      static_cast<content_settings::CookieControlsMode>(browser_pref.GetInt());
  // 3PCs are allowed iff: CookieControlsMode is not `kBlockThirdParty` and the
  // user is not in Incognito (as 3PCs are always blocked in Incognito).
  return base::Value(cookie_control_mode != kBlockThirdParty &&
                     !is_incognito_profile);
}

}  // namespace extensions
