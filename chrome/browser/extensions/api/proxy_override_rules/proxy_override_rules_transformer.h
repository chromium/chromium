// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_OVERRIDE_RULES_PROXY_OVERRIDE_RULES_TRANSFORMER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_OVERRIDE_RULES_PROXY_OVERRIDE_RULES_TRANSFORMER_H_

#include <optional>
#include <string>

#include "chrome/browser/extensions/pref_transformer_interface.h"

namespace extensions {

// Pref transformer for the chrome.proxyOverrideRulesPrivate.rules API.
class ProxyOverrideRulesTransformer : public PrefTransformerInterface {
 public:
  ProxyOverrideRulesTransformer();

  ProxyOverrideRulesTransformer(const ProxyOverrideRulesTransformer&) = delete;
  ProxyOverrideRulesTransformer& operator=(
      const ProxyOverrideRulesTransformer&) = delete;

  ~ProxyOverrideRulesTransformer() override;

  // Implementation of PrefTransformerInterface.
  std::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) override;
  std::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_OVERRIDE_RULES_PROXY_OVERRIDE_RULES_TRANSFORMER_H_
