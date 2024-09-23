// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/cookie_controls_mode_transformer.h"

#include "base/values.h"
#include "components/content_settings/core/browser/cookie_settings.h"

namespace extensions {

using CookieControlsMode = content_settings::CookieControlsMode;

CookieControlsModeTransformer::CookieControlsModeTransformer() = default;
CookieControlsModeTransformer::~CookieControlsModeTransformer() = default;

std::optional<base::Value>
CookieControlsModeTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  bool third_party_cookies_allowed = extension_pref.GetBool();
  return base::Value(static_cast<int>(
      third_party_cookies_allowed ? CookieControlsMode::kOff
                                  : CookieControlsMode::kBlockThirdParty));
}

std::optional<base::Value>
CookieControlsModeTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  auto cookie_control_mode =
      static_cast<CookieControlsMode>(browser_pref.GetInt());

  bool third_party_cookies_allowed =
      cookie_control_mode == content_settings::CookieControlsMode::kOff ||
      (!is_incognito_profile &&
       cookie_control_mode == CookieControlsMode::kIncognitoOnly);

  return base::Value(third_party_cookies_allowed);
}

}  // namespace extensions
