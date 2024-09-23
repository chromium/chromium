// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/privacy_sandbox_transformer.h"

#include "base/values.h"

namespace extensions {

PrivacySandboxTransformer::PrivacySandboxTransformer() = default;
PrivacySandboxTransformer::~PrivacySandboxTransformer() = default;

std::optional<base::Value> PrivacySandboxTransformer::ExtensionToBrowserPref(
    const base::Value& extension_pref,
    std::string& error,
    bool& bad_message) {
  if (!extension_pref.is_bool()) {
    bad_message = true;
    return std::nullopt;
  }

  if (extension_pref.GetBool()) {
    error = "Extensions aren’t allowed to enable Privacy Sandbox APIs.";
    return std::nullopt;
  }

  return extension_pref.Clone();
}

std::optional<base::Value> PrivacySandboxTransformer::BrowserToExtensionPref(
    const base::Value& browser_pref,
    bool is_incognito_profile) {
  return browser_pref.Clone();
}

}  // namespace extensions
