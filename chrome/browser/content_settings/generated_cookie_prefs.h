// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_COOKIE_PREFS_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_COOKIE_PREFS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

extern const char kCookieSessionOnly[];
extern const char kCookiePrimarySetting[];
extern const char kCookieDefaultContentSetting[];

// Must be kept in sync with the enum of the same name located in
// chrome/browser/resources/settings/privacy_page/cookies_page.js
enum class CookiePrimarySetting {
  ALLOW_ALL,
  BLOCK_THIRD_PARTY_INCOGNITO,
  BLOCK_THIRD_PARTY,
  BLOCK_ALL
};

// The base class for generated preferences which support WebUI cookie controls
// that do not not map completely to individual preferences or content settings.
// Generated preferences allows the use of these types of controls without
// exposing supporting business logic to WebUI code.
class GeneratedCookiePrefBase
    : public extensions::settings_private::GeneratedPref,
      public content_settings::Observer {
 public:
  ~GeneratedCookiePrefBase() override;

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;
  void OnCookiePreferencesChanged();

 protected:
  GeneratedCookiePrefBase(Profile* profile, const std::string& pref_name_);
  const raw_ptr<Profile> profile_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  const std::string pref_name_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};
  PrefChangeRegistrar user_prefs_registrar_;
};

class GeneratedCookiePrimarySettingPref : public GeneratedCookiePrefBase {
 public:
  explicit GeneratedCookiePrimarySettingPref(Profile* profile);

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  std::unique_ptr<extensions::api::settings_private::PrefObject> GetPrefObject()
      const override;

 private:
  // Applies the effective primary cookie setting management state from
  // |profile| to |pref_object|.
  static void ApplyPrimaryCookieSettingManagedState(
      extensions::api::settings_private::PrefObject* pref_object,
      Profile* profile);
};

class GeneratedCookieSessionOnlyPref : public GeneratedCookiePrefBase {
 public:
  explicit GeneratedCookieSessionOnlyPref(Profile* profile);

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  std::unique_ptr<extensions::api::settings_private::PrefObject> GetPrefObject()
      const override;
};

// A generated preference that represents cookies content setting and supports
// three states: allow, session only and block.
class GeneratedCookieDefaultContentSettingPref
    : public GeneratedCookiePrefBase {
 public:
  explicit GeneratedCookieDefaultContentSettingPref(Profile* profile);

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  std::unique_ptr<extensions::api::settings_private::PrefObject> GetPrefObject()
      const override;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_COOKIE_PREFS_H_
