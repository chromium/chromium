// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_JAVASCRIPT_OPTIMIZER_PREF_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_JAVASCRIPT_OPTIMIZER_PREF_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_change_registrar.h"

namespace content_settings {

extern const char kGeneratedJavascriptOptimizerPref[];

// Must be kept in sync with the JavascriptOptimizerSetting enum in
// chrome/browser/resources/settings/site_settings/constants.ts
// LINT.IfChange(JavascriptOptimizerSetting)
enum class JavascriptOptimizerSetting {
  kBlocked = 0,
  kAllowed = 1,
  kBlockedForUnfamiliarSites = 2,
};
// LINT.ThenChange(/chrome/browser/resources/settings/site_settings/constants.ts:JavascriptOptimizerSetting)

// A generated preference that represents the javascript-optimizer state and
// supports 3 states: allow, block for unfamiliar sites, and always block.
class GeneratedJavascriptOptimizerPref
    : public content_settings::Observer,
      public extensions::settings_private::GeneratedPref {
 public:
  explicit GeneratedJavascriptOptimizerPref(Profile* profile);
  ~GeneratedJavascriptOptimizerPref() override;

  // content_settings::Observer:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

  // Fired when preferences used to generate this preference are changed.
  void OnPreferencesChanged();

 private:
  // Profile this preference is generated for.
  const raw_ptr<Profile> profile_;

  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_setting_observation_{this};
  PrefChangeRegistrar user_prefs_registrar_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_JAVASCRIPT_OPTIMIZER_PREF_H_
