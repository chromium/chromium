// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PREF_TRANSFORMER_INTERFACE_H_
#define CHROME_BROWSER_EXTENSIONS_PREF_TRANSFORMER_INTERFACE_H_

#include <optional>
#include <string>

namespace base {
class Value;
}

namespace extensions {

class PrefTransformerInterface {
 public:
  virtual ~PrefTransformerInterface() = default;

  // Converts the representation of a preference as seen by the extension
  // into a representation that is used in the pref stores of the browser.
  // Returns the pref store representation in case of success or sets
  // `error` and returns std::nullopt otherwise. `bad_message` is passed
  // to simulate the behavior of EXTENSION_FUNCTION_VALIDATE.
  virtual std::optional<base::Value> ExtensionToBrowserPref(
      const base::Value& extension_pref,
      std::string& error,
      bool& bad_message) = 0;

  // Converts the representation of the preference as stored in the browser
  // into a representation that is used by the extension.
  // Returns the extension representation in case of success or returns
  // std::nullopt otherwise.
  virtual std::optional<base::Value> BrowserToExtensionPref(
      const base::Value& browser_pref,
      bool is_incognito_profile) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PREF_TRANSFORMER_INTERFACE_H_
