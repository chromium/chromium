// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/dark_light_mode_controller_impl.h"

#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/login/login_screen_controller.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "ui/chromeos/styles/cros_styles.h"

namespace ash {

namespace {

DarkLightModeControllerImpl* g_instance = nullptr;

// An array of OOBE screens which currently support dark theme.
// In the future additional screens will be added. Eventually all screens
// will support it and this array will not be needed anymore.
constexpr OobeDialogState kStatesSupportingDarkTheme[] = {
    OobeDialogState::MARKETING_OPT_IN, OobeDialogState::THEME_SELECTION,
    OobeDialogState::CHOOBE};

}  // namespace

DarkLightModeControllerImpl::DarkLightModeControllerImpl()
    : ScheduledFeature(prefs::kDarkModeEnabled,
                       prefs::kDarkModeScheduleType,
                       std::string(),
                       std::string()) {
  DCHECK(!g_instance);
  g_instance = this;

  // May be null in unit tests.
  if (Shell::HasInstance()) {
    auto* shell = Shell::Get();
    shell->login_screen_controller()->data_dispatcher()->AddObserver(this);
  }
}

DarkLightModeControllerImpl::~DarkLightModeControllerImpl() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;

  // May be null in unit tests.
  if (Shell::HasInstance()) {
    auto* shell = Shell::Get();
    auto* login_screen_controller = shell->login_screen_controller();
    auto* data_dispatcher = login_screen_controller
                                ? login_screen_controller->data_dispatcher()
                                : nullptr;
    if (data_dispatcher)
      data_dispatcher->RemoveObserver(this);
  }

  cros_styles::SetDebugColorsEnabled(false);
  cros_styles::SetDarkModeEnabled(false);
}

// static
DarkLightModeControllerImpl* DarkLightModeControllerImpl::Get() {
  return g_instance;
}

// static
void DarkLightModeControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kDarkModeScheduleType,
      static_cast<int>(ScheduleType::kSunsetToSunrise));

  registry->RegisterBooleanPref(prefs::kDarkModeEnabled,
                                kDefaultDarkModeEnabled);
}

void DarkLightModeControllerImpl::SetAutoScheduleEnabled(bool enabled) {
  SetScheduleType(enabled ? ScheduleType::kSunsetToSunrise
                          : ScheduleType::kNone);
}

bool DarkLightModeControllerImpl::GetAutoScheduleEnabled() const {
  const ScheduleType type = GetScheduleType();
  // `DarkLightModeControllerImpl` does not support the custom scheduling.
  DCHECK_NE(type, ScheduleType::kCustom);
  return type == ScheduleType::kSunsetToSunrise;
}

void DarkLightModeControllerImpl::ToggleColorMode() {
  DCHECK(active_user_pref_service_);
  active_user_pref_service_->SetBoolean(prefs::kDarkModeEnabled,
                                        !IsDarkModeEnabled());
  active_user_pref_service_->CommitPendingWrite();
  NotifyColorModeChanges();
}

void DarkLightModeControllerImpl::AddObserver(ColorModeObserver* observer) {
  observers_.AddObserver(observer);
}

void DarkLightModeControllerImpl::RemoveObserver(ColorModeObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DarkLightModeControllerImpl::IsDarkModeEnabled() const {
  // Dark mode is off during OOBE when the OobeDialogState is still unknown.
  // When the SessionState is OOBE, the OobeDialogState is HIDDEN until the
  // first screen is shown. This fixes a bug that caused dark colors to be
  // flashed when OOBE is loaded. See b/260008998
  const auto session_state =
      Shell::Get()->session_controller()->GetSessionState();
  if (oobe_state_ == OobeDialogState::HIDDEN &&
      session_state == session_manager::SessionState::OOBE) {
    return false;
  }

  // Disable dark mode for Shimless RMA.
  if (session_state == session_manager::SessionState::RMA) {
    return false;
  }

  if (is_dark_mode_enabled_in_oobe_for_testing_.has_value()) {
    return is_dark_mode_enabled_in_oobe_for_testing_.value();
  }

  if (oobe_state_ != OobeDialogState::HIDDEN) {
    if (active_user_pref_service_) {
      const PrefService::Preference* pref =
          active_user_pref_service_->FindPreference(
              prefs::kDarkModeScheduleType);
      // Managed users do not see the theme selection screen, so to avoid
      // confusion they should always see light colors during OOBE
      if (pref->IsManaged() || pref->IsRecommended()) {
        return false;
      }

      if (!active_user_pref_service_->GetBoolean(prefs::kDarkModeEnabled)) {
        return false;
      }
    }
    return base::Contains(kStatesSupportingDarkTheme, oobe_state_);
  }

  // On the login screen use the preference of the focused pod's user if they
  // had the preference stored in the known_user and the pod is focused.
  if (!active_user_pref_service_ &&
      is_dark_mode_enabled_for_focused_pod_.has_value()) {
    return is_dark_mode_enabled_for_focused_pod_.value();
  }

  // Keep the color mode as DARK in login screen.
  if (!active_user_pref_service_) {
    return true;
  }

  return active_user_pref_service_->GetBoolean(prefs::kDarkModeEnabled);
}

void DarkLightModeControllerImpl::SetDarkModeEnabledForTest(bool enabled) {
  if (oobe_state_ != OobeDialogState::HIDDEN) {
    auto closure = GetNotifyOnDarkModeChangeClosure();
    is_dark_mode_enabled_in_oobe_for_testing_ = enabled;
    return;
  }

  if (IsDarkModeEnabled() != enabled) {
    ToggleColorMode();
  }
}

void DarkLightModeControllerImpl::OnOobeDialogStateChanged(
    OobeDialogState state) {
  auto closure = GetNotifyOnDarkModeChangeClosure();
  oobe_state_ = state;
}

void DarkLightModeControllerImpl::OnFocusPod(const AccountId& account_id) {
  auto closure = GetNotifyOnDarkModeChangeClosure();

  if (!account_id.is_valid()) {
    is_dark_mode_enabled_for_focused_pod_.reset();
    return;
  }
  is_dark_mode_enabled_for_focused_pod_ =
      user_manager::KnownUser(ash::Shell::Get()->local_state())
          .FindBoolPath(account_id, prefs::kDarkModeEnabled);
}

void DarkLightModeControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  active_user_pref_service_ = prefs;
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);

  pref_change_registrar_->Add(
      prefs::kDarkModeEnabled,
      base::BindRepeating(&DarkLightModeControllerImpl::NotifyColorModeChanges,
                          base::Unretained(this)));

  // Immediately tell all the observers to load this user's saved preferences.
  NotifyColorModeChanges();

  ScheduledFeature::OnActiveUserPrefServiceChanged(prefs);
}

void DarkLightModeControllerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  auto closure = GetNotifyOnDarkModeChangeClosure();
  if (state != session_manager::SessionState::OOBE &&
      state != session_manager::SessionState::LOGIN_PRIMARY) {
    oobe_state_ = OobeDialogState::HIDDEN;
  }
}

const char* DarkLightModeControllerImpl::GetFeatureName() const {
  return "DarkLightModeControllerImpl";
}

void DarkLightModeControllerImpl::NotifyColorModeChanges() {
  const bool is_enabled = IsDarkModeEnabled();
  cros_styles::SetDarkModeEnabled(is_enabled);
  if (last_value_ == is_enabled) {
    // Updating the pref causes a notification. Skip it if it happens.
    return;
  }
  last_value_ = is_enabled;
  for (auto& observer : observers_) {
    observer.OnColorModeChanged(is_enabled);
  }
}

base::ScopedClosureRunner
DarkLightModeControllerImpl::GetNotifyOnDarkModeChangeClosure() {
  return base::ScopedClosureRunner(
      // Unretained is safe here because GetNotifyOnDarkModeChangeClosure is a
      // private function and callback should be called on going out of scope of
      // the calling method.
      base::BindOnce(&DarkLightModeControllerImpl::NotifyIfDarkModeChanged,
                     base::Unretained(this), IsDarkModeEnabled()));
}

void DarkLightModeControllerImpl::NotifyIfDarkModeChanged(
    bool old_is_dark_mode_enabled) {
  // If this is the first check, always notify.
  if (last_value_.has_value() &&
      old_is_dark_mode_enabled == IsDarkModeEnabled()) {
    return;
  }
  NotifyColorModeChanges();
}

}  // namespace ash
