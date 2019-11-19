// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/chromeos_resolve_time_zone_by_geolocation_on_off.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/extensions/api/settings_private/generated_time_zone_pref_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace extensions {

namespace settings_api = api::settings_private;

namespace settings_private {
namespace {

// Implements kResolveTimezoneByGeolocationOnOff generated preference.
class GeneratedResolveTimezoneByGeolocationOnOff
    : public GeneratedTimeZonePrefBase {
 public:
  explicit GeneratedResolveTimezoneByGeolocationOnOff(Profile* profile);
  ~GeneratedResolveTimezoneByGeolocationOnOff() override;

  // GeneratedPref implementation:
  std::unique_ptr<settings_api::PrefObject> GetPrefObject() const override;
  SetPrefResult SetPref(const base::Value* value) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GeneratedResolveTimezoneByGeolocationOnOff);
};

GeneratedResolveTimezoneByGeolocationOnOff::
    GeneratedResolveTimezoneByGeolocationOnOff(Profile* profile)
    : GeneratedTimeZonePrefBase(kResolveTimezoneByGeolocationOnOff, profile) {}

GeneratedResolveTimezoneByGeolocationOnOff::
    ~GeneratedResolveTimezoneByGeolocationOnOff() = default;

std::unique_ptr<settings_api::PrefObject>
GeneratedResolveTimezoneByGeolocationOnOff::GetPrefObject() const {
  std::unique_ptr<settings_api::PrefObject> pref_object =
      std::make_unique<settings_api::PrefObject>();

  pref_object->key = pref_name_;
  pref_object->type = settings_api::PREF_TYPE_BOOLEAN;
  pref_object->value =
      std::make_unique<base::Value>(g_browser_process->platform_part()
                                        ->GetTimezoneResolverManager()
                                        ->TimeZoneResolverShouldBeRunning());

  UpdateTimeZonePrefControlledBy(pref_object.get());

  return pref_object;
}

SetPrefResult GeneratedResolveTimezoneByGeolocationOnOff::SetPref(
    const base::Value* value) {
  if (!value->is_bool())
    return SetPrefResult::PREF_TYPE_MISMATCH;

  // Check if preference is policy or primary-user controlled, and therefore
  // cannot deactivate automatic timezone.
  if (chromeos::system::TimeZoneResolverManager::
          IsTimeZoneResolutionPolicyControlled() ||
      !profile_->IsSameProfile(ProfileManager::GetPrimaryUserProfile())) {
    return SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  // If the Parent Access Code feature is not available, children must use
  // automatic timezone.
  if (profile_->IsChild() &&
      !base::FeatureList::IsEnabled(features::kParentAccessCodeForTimeChange)) {
    return SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  const bool new_value = value->GetBool();
  const bool current_value = g_browser_process->platform_part()
                                 ->GetTimezoneResolverManager()
                                 ->TimeZoneResolverShouldBeRunning();
  if (new_value == current_value)
    return SetPrefResult::SUCCESS;

  profile_->GetPrefs()->SetInteger(
      ::prefs::kResolveTimezoneByGeolocationMethod,
      static_cast<int>(new_value ? chromeos::system::TimeZoneResolverManager::
                                       TimeZoneResolveMethod::IP_ONLY
                                 : chromeos::system::TimeZoneResolverManager::
                                       TimeZoneResolveMethod::DISABLED));

  return SetPrefResult::SUCCESS;
}

}  //  anonymous namespace

const char kResolveTimezoneByGeolocationOnOff[] =
    "generated.resolve_timezone_by_geolocation_on_off";

std::unique_ptr<GeneratedPref> CreateGeneratedResolveTimezoneByGeolocationOnOff(
    Profile* profile) {
  return std::make_unique<GeneratedResolveTimezoneByGeolocationOnOff>(profile);
}

}  // namespace settings_private
}  // namespace extensions
