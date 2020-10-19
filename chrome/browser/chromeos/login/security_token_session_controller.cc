// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/security_token_session_controller.h"

#include "base/bind.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace login {

namespace {

// Possible values of prefs::kSecurityTokenSessionBehavior. This needs to match
// the values of the SecurityTokenSessionBehavior policy defined in
// policy_templates.json.
constexpr char kIgnorePrefValue[] = "IGNORE";
constexpr char kLogoutPrefValue[] = "LOGOUT";
constexpr char kLockPrefValue[] = "LOCK";

SecurityTokenSessionController::Behavior ParseBehaviorPrefValue(
    const std::string& behavior) {
  if (behavior == kIgnorePrefValue)
    return SecurityTokenSessionController::Behavior::kIgnore;
  if (behavior == kLogoutPrefValue)
    return SecurityTokenSessionController::Behavior::kLogout;
  if (behavior == kLockPrefValue)
    return SecurityTokenSessionController::Behavior::kLock;

  return SecurityTokenSessionController::Behavior::kIgnore;
}

}  // namespace

SecurityTokenSessionController::SecurityTokenSessionController(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  UpdateNotificationPref();
  UpdateBehaviorPref();
  pref_change_registrar_.Init(pref_service_);
  base::RepeatingClosure behavior_pref_changed_callback =
      base::BindRepeating(&SecurityTokenSessionController::UpdateBehaviorPref,
                          base::Unretained(this));
  base::RepeatingClosure notification_pref_changed_callback =
      base::BindRepeating(
          &SecurityTokenSessionController::UpdateNotificationPref,
          base::Unretained(this));
  pref_change_registrar_.Add(prefs::kSecurityTokenSessionBehavior,
                             behavior_pref_changed_callback);
  pref_change_registrar_.Add(prefs::kSecurityTokenSessionNotificationSeconds,
                             notification_pref_changed_callback);
}

SecurityTokenSessionController::~SecurityTokenSessionController() = default;

void SecurityTokenSessionController::Shutdown() {
  pref_change_registrar_.RemoveAll();
}

// static
void SecurityTokenSessionController::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kSecurityTokenSessionBehavior,
                               kIgnorePrefValue);
  registry->RegisterIntegerPref(prefs::kSecurityTokenSessionNotificationSeconds,
                                0);
}

void SecurityTokenSessionController::UpdateBehaviorPref() {
  behavior_ = GetBehaviorFromPref();
}

void SecurityTokenSessionController::UpdateNotificationPref() {
  notification_seconds_ =
      base::TimeDelta::FromSeconds(pref_service_->GetInteger(
          prefs::kSecurityTokenSessionNotificationSeconds));
}

SecurityTokenSessionController::Behavior
SecurityTokenSessionController::GetBehaviorFromPref() const {
  return ParseBehaviorPrefValue(
      pref_service_->GetString(prefs::kSecurityTokenSessionBehavior));
}

}  // namespace login
}  // namespace chromeos
