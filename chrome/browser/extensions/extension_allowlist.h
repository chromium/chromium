// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ALLOWLIST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ALLOWLIST_H_

#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace base {
class Value;
}  // namespace base

namespace extensions {
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionService;

// Manages the Safe Browsing CRX Allowlist.
class ExtensionAllowlist {
 public:
  ExtensionAllowlist(Profile* profile,
                     ExtensionPrefs* extension_prefs,
                     ExtensionService* extension_service);
  ExtensionAllowlist(const ExtensionAllowlist&) = delete;
  ExtensionAllowlist& operator=(const ExtensionAllowlist&) = delete;
  ~ExtensionAllowlist() = default;

  void Init();

  // Performs action based on Omaha attributes for the extension.
  void PerformActionBasedOnOmahaAttributes(const std::string& extension_id,
                                           const base::Value& attributes);

  // Whether a warning should be displayed for an extension, `true` if the
  // extension is not allowlisted and the allowlist is enforced.
  bool ShouldDisplayWarning(const std::string& extension_id) const;

 private:
  // Set if the allowlist should be enforced or not.
  void SetAllowlistEnforcedField();

  // Blocklist all extensions with allowlist state `ALLOWLIST_NOT_ALLOWLISTED`.
  void ActivateAllowlistEnforcement();

  // Unblocklist all extensions with allowlist state
  // |ALLOWLIST_NOT_ALLOWLISTED|.
  void DeactivateAllowlistEnforcement();

  // Called when the 'Enhanced Safe Browsing' setting changes.
  void OnSafeBrowsingEnhancedChanged();

  Profile* profile_ = nullptr;
  ExtensionPrefs* extension_prefs_ = nullptr;
  ExtensionService* extension_service_ = nullptr;
  ExtensionRegistry* registry_ = nullptr;

  // Whether the Safe Browsing allowlist is currently enforced or not.
  bool is_allowlist_enforced_ = false;

  // Used to subscribe to profile preferences updates.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ALLOWLIST_H_
