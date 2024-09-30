// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/generated_prefs.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/generated_cookie_prefs.h"
#include "chrome/browser/content_settings/generated_permission_prompting_behavior_pref.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util_enums.h"
#include "chrome/browser/password_manager/generated_password_leak_detection_pref.h"
#include "chrome/browser/safe_browsing/generated_safe_browsing_pref.h"
#include "chrome/browser/ssl/generated_https_first_mode_pref.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_method_short.h"
#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_on_off.h"
#endif

namespace extensions {
namespace settings_private {

GeneratedPrefs::GeneratedPrefs(Profile* profile) : profile_(profile) {}

GeneratedPrefs::~GeneratedPrefs() = default;

bool GeneratedPrefs::HasPref(const std::string& pref_name) {
  return FindPrefImpl(pref_name) != nullptr;
}

std::optional<api::settings_private::PrefObject> GeneratedPrefs::GetPref(
    const std::string& pref_name) {
  GeneratedPref* impl = FindPrefImpl(pref_name);
  if (!impl)
    return std::nullopt;

  return impl->GetPrefObject();
}

SetPrefResult GeneratedPrefs::SetPref(const std::string& pref_name,
                                      const base::Value* value) {
  GeneratedPref* impl = FindPrefImpl(pref_name);
  if (!impl)
    return SetPrefResult::PREF_NOT_FOUND;

  return impl->SetPref(value);
}

void GeneratedPrefs::AddObserver(const std::string& pref_name,
                                 GeneratedPref::Observer* observer) {
  GeneratedPref* impl = FindPrefImpl(pref_name);
  CHECK(impl);

  impl->AddObserver(observer);
}

void GeneratedPrefs::RemoveObserver(const std::string& pref_name,
                                    GeneratedPref::Observer* observer) {
  GeneratedPref* impl = FindPrefImpl(pref_name);
  if (!impl)
    return;

  impl->RemoveObserver(observer);
}

void GeneratedPrefs::Shutdown() {
  // Clear preference map so generated prefs are destroyed before services they
  // may depend on are shutdown.
  prefs_.clear();
}

GeneratedPref* GeneratedPrefs::FindPrefImpl(const std::string& pref_name) {
  if (prefs_.empty())
    CreatePrefs();

  const PrefsMap::const_iterator it = prefs_.find(pref_name);
  if (it == prefs_.end())
    return nullptr;

  return it->second.get();
}

void GeneratedPrefs::CreatePrefs() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  prefs_[kResolveTimezoneByGeolocationOnOff] =
      CreateGeneratedResolveTimezoneByGeolocationOnOff(profile_);
  prefs_[kResolveTimezoneByGeolocationMethodShort] =
      CreateGeneratedResolveTimezoneByGeolocationMethodShort(profile_);
#endif
  prefs_[content_settings::kCookieDefaultContentSetting] = std::make_unique<
      content_settings::GeneratedCookieDefaultContentSettingPref>(profile_);
  prefs_[kGeneratedPasswordLeakDetectionPref] =
      std::make_unique<GeneratedPasswordLeakDetectionPref>(profile_);
  prefs_[safe_browsing::kGeneratedSafeBrowsingPref] =
      std::make_unique<safe_browsing::GeneratedSafeBrowsingPref>(profile_);
  prefs_[content_settings::kGeneratedNotificationPref] = std::make_unique<
      content_settings::GeneratedPermissionPromptingBehaviorPref>(
      profile_, ContentSettingsType::NOTIFICATIONS);
  prefs_[content_settings::kGeneratedGeolocationPref] = std::make_unique<
      content_settings::GeneratedPermissionPromptingBehaviorPref>(
      profile_, ContentSettingsType::GEOLOCATION);
  prefs_[kGeneratedHttpsFirstModePref] =
      std::make_unique<GeneratedHttpsFirstModePref>(profile_);
}

}  // namespace settings_private
}  // namespace extensions
