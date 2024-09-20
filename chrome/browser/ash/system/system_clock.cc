// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system/system_clock.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system/system_clock_observer.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace ash::system {

SystemClock::SystemClock() {
  device_settings_observer_ = CrosSettings::Get()->AddSettingsObserver(
      kSystemUse24HourClock,
      base::BindRepeating(&SystemClock::OnSystemPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  user_manager::UserManager::Get()->AddSessionStateObserver(this);
}

SystemClock::~SystemClock() {
  if (user_manager::UserManager::IsInitialized())
    user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
}

void SystemClock::OnProfileWillBeDestroyed(Profile* profile) {
  DCHECK(profile_observation_.IsObservingSource(profile));
  profile_observation_.Reset();
  user_pref_registrar_.reset();
}

void SystemClock::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user)
    return;

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&SystemClock::SetProfileByUser,
                     weak_ptr_factory_.GetWeakPtr(), active_user));
}

void SystemClock::AddObserver(SystemClockObserver* observer) {
  observer_list_.AddObserver(observer);
}

void SystemClock::RemoveObserver(SystemClockObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void SystemClock::SetProfileByUser(const user_manager::User* user) {
  SetProfile(ProfileHelper::Get()->GetProfileByUser(user));
}

void SystemClock::SetProfile(Profile* profile) {
  profile_observation_.Reset();
  profile_observation_.Observe(profile);
  PrefService* prefs = profile->GetPrefs();
  user_pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  user_pref_registrar_->Init(prefs);
  user_pref_registrar_->Add(prefs::kUse24HourClock,
                            base::BindRepeating(&SystemClock::OnUserPrefChanged,
                                                base::Unretained(this)));
  NotifySystemClockTypeChanged();
}

SystemClock::ScopedHourClockType::ScopedHourClockType(
    base::WeakPtr<SystemClock> system_clock)
    : system_clock_(std::move(system_clock)) {}

SystemClock::ScopedHourClockType::~ScopedHourClockType() {
  if (!system_clock_)
    return;
  system_clock_->scoped_hour_clock_type_.reset();
  system_clock_->NotifySystemClockTypeChanged();
}

SystemClock::ScopedHourClockType::ScopedHourClockType(
    ScopedHourClockType&& other) = default;

SystemClock::ScopedHourClockType& SystemClock::ScopedHourClockType::operator=(
    ScopedHourClockType&& other) = default;

void SystemClock::ScopedHourClockType::UpdateClockType(
    base::HourClockType clock_type) {
  if (!system_clock_)
    return;
  system_clock_->scoped_hour_clock_type_ = clock_type;
  system_clock_->NotifySystemClockTypeChanged();
}

SystemClock::ScopedHourClockType SystemClock::CreateScopedHourClockType(
    base::HourClockType hour_clock_type) {
  DCHECK(!scoped_hour_clock_type_.has_value());
  scoped_hour_clock_type_ = hour_clock_type;
  NotifySystemClockTypeChanged();
  return ScopedHourClockType(weak_ptr_factory_.GetWeakPtr());
}

bool SystemClock::ShouldUse24HourClockForUser(const AccountId& user_id) const {
  // System value is used for kUse24HourClock preference on login screen and
  // whenever set so in user's preference
  const CrosSettings* const cros_settings = CrosSettings::Get();
  bool system_use_24_hour_clock = true;
  const bool system_value_found = cros_settings->GetBoolean(
      kSystemUse24HourClock, &system_use_24_hour_clock);
  const bool system_value =
      system_value_found ? system_use_24_hour_clock
                         : (base::GetHourClockType() == base::k24HourClock);

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(user_id);
  if (!user || !user->GetProfilePrefs()) {
    return system_value;
  }

  const PrefService::Preference* user_pref =
      user->GetProfilePrefs()->FindPreference(prefs::kUse24HourClock);
  if (user_pref->IsDefaultValue()) {
    // Non-regular users or owner use `system_value`. Owner uses `system_value`
    // to mitigate the edge case where owner data is lost because `system_value`
    // should always be consistent with the (lost) owner data.
    if (user->GetType() != user_manager::UserType::kRegular ||
        user_manager::UserManager::Get()->IsOwnerUser(user)) {
      return system_value;
    }

    // Regular users use `system_value` on managed devices where `system_value`
    // is from device policy.
    DCHECK_EQ(user->GetType(), user_manager::UserType::kRegular);
    if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
      return system_value;
    }
  }

  return user_pref->GetValue()->GetIfBool().value_or(true);
}

bool SystemClock::ShouldUse24HourClock() const {
  if (scoped_hour_clock_type_.has_value()) {
    return scoped_hour_clock_type_ == base::k24HourClock;
  }

  user_manager::User* const active_user =
      user_manager::UserManager::Get()->GetActiveUser();

  return ShouldUse24HourClockForUser(active_user ? active_user->GetAccountId()
                                                 : EmptyAccountId());
}

void SystemClock::OnSystemPrefChanged() {
  NotifySystemClockTypeChanged();
}

void SystemClock::OnUserPrefChanged() {
  // It apparently sometimes takes a while after login before the current user
  // is recognized as the owner. Make sure that the system-wide clock setting
  // is updated when the recognition eventually happens (crbug.com/278601).
  // This also works for enterprise-managed devices because they never have
  // a local owner.
  const AccountId* user_id =
      ash::AnnotatedAccountId::Get(profile_observation_.GetSource());
  if (user_id) {
    user_manager::UserManager::Get()->GetOwnerAccountIdAsync(
        base::BindOnce(&SystemClock::MaybeCopyToOwnerSettings,
                       weak_ptr_factory_.GetWeakPtr(), *user_id));
  }

  NotifySystemClockTypeChanged();
}

void SystemClock::MaybeCopyToOwnerSettings(const AccountId& updating_user_id,
                                           const AccountId& owner_id) {
  if (updating_user_id != owner_id) {
    // Non owner user.
    return;
  }

  const bool use_24_hour_clock = ShouldUse24HourClockForUser(updating_user_id);

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(updating_user_id);
  if (!user) {
    return;  // May occur if not running on a device.
  }
  Profile* const profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile) {
    return;  // May occur in tests or if not running on a device.
  }
  OwnerSettingsServiceAsh* const service =
      OwnerSettingsServiceAshFactory::GetForBrowserContext(profile);
  CHECK(service);
  service->SetBoolean(kSystemUse24HourClock, use_24_hour_clock);
}

void SystemClock::NotifySystemClockTypeChanged() {
  for (auto& observer : observer_list_)
    observer.OnSystemClockChanged(this);
}

}  // namespace ash::system
