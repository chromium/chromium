// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ALLOWLIST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ALLOWLIST_H_

#include "base/scoped_observation.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_observer.h"

class Profile;

namespace base {
class Value;
}  // namespace base

namespace extensions {
class ExtensionRegistry;
class ExtensionService;

// Manages the Safe Browsing CRX Allowlist.
class ExtensionAllowlist : private ExtensionPrefsObserver {
 public:
  ExtensionAllowlist(Profile* profile,
                     ExtensionPrefs* extension_prefs,
                     ExtensionService* extension_service);
  ExtensionAllowlist(const ExtensionAllowlist&) = delete;
  ExtensionAllowlist& operator=(const ExtensionAllowlist&) = delete;
  ~ExtensionAllowlist();

  void Init();

  // Performs action based on Omaha attributes for the extension.
  void PerformActionBasedOnOmahaAttributes(const std::string& extension_id,
                                           const base::Value& attributes);

  // Whether a warning should be displayed for an extension, `true` if the
  // extension is not allowlisted and the allowlist is enforced.
  bool ShouldDisplayWarning(const std::string& extension_id) const;

  // Whether the ESB allowlist is enforced or not.
  bool is_allowlist_enforced() { return is_allowlist_enforced_; }

 private:
  // Set if the allowlist should be enforced or not.
  void SetAllowlistEnforcedField();

  // Apply the allowlist enforcement by disabling a not allowlisted extension if
  // allowed by policy.
  void ApplyEnforcement(const std::string& extension_id);

  // Blocklist all extensions with allowlist state `ALLOWLIST_NOT_ALLOWLISTED`.
  void ActivateAllowlistEnforcement();

  // Unblocklist all extensions with allowlist state
  // `ALLOWLIST_NOT_ALLOWLISTED`.
  void DeactivateAllowlistEnforcement();

  // Called when the 'Enhanced Safe Browsing' setting changes.
  void OnSafeBrowsingEnhancedChanged();

  // ExtensionPrefsObserver:
  // Observes extension state changes to set
  // `ALLOWLIST_ACKNOWLEDGE_ENABLED_BY_USER` when a not allowlisted extension is
  // re-enabled by the user.
  void OnExtensionStateChanged(const std::string& extension_id,
                               bool is_now_enabled) override;

  Profile* profile_ = nullptr;
  ExtensionPrefs* extension_prefs_ = nullptr;
  ExtensionService* extension_service_ = nullptr;
  ExtensionRegistry* registry_ = nullptr;

  // Whether the Safe Browsing allowlist is currently enforced or not.
  bool is_allowlist_enforced_ = false;

  // Used to subscribe to profile preferences updates.
  PrefChangeRegistrar pref_change_registrar_;

  base::ScopedObservation<ExtensionPrefs, ExtensionPrefsObserver>
      extension_prefs_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ALLOWLIST_H_
