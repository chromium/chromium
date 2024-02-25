// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_method_short.h"

#include "chrome/browser/ash/system/timezone_resolver_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/extensions/api/settings_private/generated_time_zone_pref_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace extensions {

namespace settings_api = api::settings_private;

namespace settings_private {
namespace {

// Implements kResolveTimezoneByGeolocationMethodShort generated preference.
class GeneratedResolveTimezoneByGeolocationMethodShort
    : public GeneratedTimeZonePrefBase {
 public:
  explicit GeneratedResolveTimezoneByGeolocationMethodShort(Profile* profile);

  GeneratedResolveTimezoneByGeolocationMethodShort(
      const GeneratedResolveTimezoneByGeolocationMethodShort&) = delete;
  GeneratedResolveTimezoneByGeolocationMethodShort& operator=(
      const GeneratedResolveTimezoneByGeolocationMethodShort&) = delete;

  ~GeneratedResolveTimezoneByGeolocationMethodShort() override;

  // GeneratedPrefsChromeOSImpl implementation:
  settings_api::PrefObject GetPrefObject() const override;
  SetPrefResult SetPref(const base::Value* value) override;
};

GeneratedResolveTimezoneByGeolocationMethodShort::
    GeneratedResolveTimezoneByGeolocationMethodShort(Profile* profile)
    : GeneratedTimeZonePrefBase(kResolveTimezoneByGeolocationMethodShort,
                                profile) {}

GeneratedResolveTimezoneByGeolocationMethodShort::
    ~GeneratedResolveTimezoneByGeolocationMethodShort() = default;

settings_api::PrefObject
GeneratedResolveTimezoneByGeolocationMethodShort::GetPrefObject() const {
  settings_api::PrefObject pref_object;

  pref_object.key = pref_name_;
  pref_object.type = settings_api::PrefType::kNumber;
  pref_object.value = base::Value(static_cast<int>(
      g_browser_process->platform_part()
          ->GetTimezoneResolverManager()
          ->GetEffectiveUserTimeZoneResolveMethod(profile_->GetPrefs(), true)));
  UpdateTimeZonePrefControlledBy(&pref_object);

  return pref_object;
}

SetPrefResult GeneratedResolveTimezoneByGeolocationMethodShort::SetPref(
    const base::Value* value) {
  if (!value->is_int())
    return SetPrefResult::PREF_TYPE_MISMATCH;

  // Check if preference is policy or primary-user controlled.
  if (ash::system::TimeZoneResolverManager::
          IsTimeZoneResolutionPolicyControlled() ||
      !profile_->IsSameOrParent(ProfileManager::GetPrimaryUserProfile())) {
    return SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  // Check if automatic time zone detection is disabled.
  // (kResolveTimezoneByGeolocationOnOff must be modified first.)
  if (!g_browser_process->platform_part()
           ->GetTimezoneResolverManager()
           ->TimeZoneResolverAllowedByTimeZoneConfigData()) {
    return SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  const ash::system::TimeZoneResolverManager::TimeZoneResolveMethod new_value =
      ash::system::TimeZoneResolverManager::TimeZoneResolveMethodFromInt(
          value->GetInt());
  const ash::system::TimeZoneResolverManager::TimeZoneResolveMethod
      current_value = g_browser_process->platform_part()
                          ->GetTimezoneResolverManager()
                          ->GetEffectiveUserTimeZoneResolveMethod(
                              profile_->GetPrefs(), true);
  if (new_value == current_value)
    return SetPrefResult::SUCCESS;

  profile_->GetPrefs()->SetInteger(::prefs::kResolveTimezoneByGeolocationMethod,
                                   static_cast<int>(new_value));

  return SetPrefResult::SUCCESS;
}

}  // anonymous namespace

const char kResolveTimezoneByGeolocationMethodShort[] =
    "generated.resolve_timezone_by_geolocation_method_short";

std::unique_ptr<GeneratedPref>
CreateGeneratedResolveTimezoneByGeolocationMethodShort(Profile* profile) {
  return std::make_unique<GeneratedResolveTimezoneByGeolocationMethodShort>(
      profile);
}

}  // namespace settings_private
}  // namespace extensions
