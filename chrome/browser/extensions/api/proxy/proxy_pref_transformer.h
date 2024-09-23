// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_PREF_TRANSFORMER_H_
#define CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_PREF_TRANSFORMER_H_

#include <optional>
#include <string>

#include "chrome/browser/extensions/pref_transformer_interface.h"

namespace base {
class Value;
}  // namespace base

namespace extensions {

// Class to convert between the representation of proxy settings used
// in the Proxy Settings API and the representation used in the PrefStores.
// This plugs into the ExtensionPreferenceAPI to get and set proxy settings.
class ProxyPrefTransformer : public PrefTransformerInterface {
 public:
  ProxyPrefTransformer();

  ProxyPrefTransformer(const ProxyPrefTransformer&) = delete;
  ProxyPrefTransformer& operator=(const ProxyPrefTransformer&) = delete;

  ~ProxyPrefTransformer() override;

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

#endif  // CHROME_BROWSER_EXTENSIONS_API_PROXY_PROXY_PREF_TRANSFORMER_H_
