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
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

extern const char kCookieDefaultContentSetting[];
extern const char kThirdPartyCookieBlockingSetting[];

enum class ThirdPartyCookieBlockingSetting {
  BLOCK_THIRD_PARTY,
  INCOGNITO_ONLY
};

// A generated preference that represents cookies content setting and supports
// three states: allow, session only and block.
// Using a generated pref allows us to support these controls without exposing
// supporting business logic to WebUI code.
class GeneratedCookieDefaultContentSettingPref
    : public extensions::settings_private::GeneratedPref,
      public content_settings::Observer {
 public:
  explicit GeneratedCookieDefaultContentSettingPref(Profile* profile);
  ~GeneratedCookieDefaultContentSettingPref() override;

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

 private:
  const raw_ptr<Profile> profile_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_settings_observation_{this};
};

class GeneratedThirdPartyCookieBlockingSettingPref
    : public extensions::settings_private::GeneratedPref,
      public content_settings::Observer {
 public:
  explicit GeneratedThirdPartyCookieBlockingSettingPref(Profile* profile);
  ~GeneratedThirdPartyCookieBlockingSettingPref() override;

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

 private:
  extensions::settings_private::SetPrefResult SetPrefResult(
      CookieControlsMode value);
  ThirdPartyCookieBlockingSetting GetValue() const;

  const raw_ptr<Profile> profile_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_COOKIE_PREFS_H_
