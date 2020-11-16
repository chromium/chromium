// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/security_token_session_controller.h"

#include <string>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromeos {
namespace login {

namespace {

// Possible values of prefs::kSecurityTokenSessionBehavior. This needs to match
// the values of the SecurityTokenSessionBehavior policy defined in
// policy_templates.json.
constexpr char kIgnorePrefValue[] = "IGNORE";
constexpr char kLogoutPrefValue[] = "LOGOUT";
constexpr char kLockPrefValue[] = "LOCK";

constexpr char kNotifierSecurityTokenSession[] =
    "ash.security_token_session_controller";
constexpr char kNotificationId[] =
    "security_token_session_controller_notification";

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

std::string GetEnterpriseDomainFromEmail(const std::string& email) {
  size_t email_separator_pos = email.find('@');
  bool is_email = email_separator_pos != std::string::npos &&
                  email_separator_pos < email.length() - 1;

  if (!is_email)
    return std::string();

  return gaia::ExtractDomainName(email);
}

// Checks if `domain` represents a valid domain. Returns false if `domain` is
// malformed. Returns the host part, which should be displayed to the user, in
// `sanitized_domain`.
bool SanitizeDomain(const std::string& domain, std::string& sanitized_domain) {
  // Add "http://" to the url. Otherwise, "example.com" would be rejected,
  // even though it has the format that is expected for `domain`.
  GURL url(std::string(url::kHttpScheme) +
           std::string(url::kStandardSchemeSeparator) + domain);
  if (!url.is_valid())
    return false;
  if (!url.has_host())
    return false;
  sanitized_domain = url.host();
  return true;
}

void DisplayNotification(const base::string16& title,
                         const base::string16& text) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title,
          text,
          /*display_source=*/base::string16(), /*origin_url=*/GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierSecurityTokenSession),
          /*optional_fields=*/{},
          new message_center::HandleNotificationClickDelegate(
              base::DoNothing::Repeatedly()),
          chromeos::kEnterpriseIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->set_fullscreen_visibility(
      message_center::FullscreenVisibility::OVER_USER);
  notification->SetSystemPriority();
  SystemNotificationHelper::GetInstance()->Display(*notification);
}

}  // namespace

SecurityTokenSessionController::SecurityTokenSessionController(
    PrefService* local_state,
    PrefService* profile_prefs,
    const user_manager::User* user)
    : local_state_(local_state), profile_prefs_(profile_prefs), user_(user) {
  DCHECK(local_state_);
  DCHECK(profile_prefs_);
  DCHECK(user_);
  UpdateNotificationPref();
  UpdateBehaviorPref();
  pref_change_registrar_.Init(profile_prefs_);
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
void SecurityTokenSessionController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kSecurityTokenSessionNotificationScheduledDomain, "");
}

// static
void SecurityTokenSessionController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kSecurityTokenSessionBehavior,
                               kIgnorePrefValue);
  registry->RegisterIntegerPref(prefs::kSecurityTokenSessionNotificationSeconds,
                                0);
  registry->RegisterBooleanPref(
      prefs::kSecurityTokenSessionNotificationDisplayed, false);
}

// static
void SecurityTokenSessionController::MaybeDisplayLoginScreenNotification() {
  PrefService* local_state = g_browser_process->local_state();
  const PrefService::Preference* scheduled_notification_domain =
      local_state->FindPreference(
          prefs::kSecurityTokenSessionNotificationScheduledDomain);
  if (!scheduled_notification_domain ||
      scheduled_notification_domain->IsDefaultValue() ||
      !scheduled_notification_domain->GetValue()->is_string()) {
    // No notification is scheduled.
    return;
  }
  local_state->ClearPref(
      prefs::kSecurityTokenSessionNotificationScheduledDomain);
  // Sanitize `scheduled_notification_domain`, as values coming from local state
  // are not trusted.
  std::string sanitized_domain;
  if (!SanitizeDomain(scheduled_notification_domain->GetValue()->GetString(),
                      sanitized_domain)) {
    // The pref value is invalid.
    return;
  }
  DisplayNotification(
      l10n_util::GetStringUTF16(
          IDS_SECURITY_TOKEN_SESSION_LOGOUT_MESSAGE_TITLE),
      l10n_util::GetStringFUTF16(IDS_SECURITY_TOKEN_SESSION_LOGOUT_MESSAGE_BODY,
                                 base::UTF8ToUTF16(sanitized_domain)));
}

void SecurityTokenSessionController::UpdateBehaviorPref() {
  behavior_ = GetBehaviorFromPref();
}

void SecurityTokenSessionController::UpdateNotificationPref() {
  notification_seconds_ =
      base::TimeDelta::FromSeconds(profile_prefs_->GetInteger(
          prefs::kSecurityTokenSessionNotificationSeconds));
}

SecurityTokenSessionController::Behavior
SecurityTokenSessionController::GetBehaviorFromPref() const {
  return ParseBehaviorPrefValue(
      profile_prefs_->GetString(prefs::kSecurityTokenSessionBehavior));
}

void SecurityTokenSessionController::AddLockNotification() const {
  // A user should see the notification only the first time their session is
  // locked.
  if (profile_prefs_->GetBoolean(
          prefs::kSecurityTokenSessionNotificationDisplayed)) {
    return;
  }
  profile_prefs_->SetBoolean(prefs::kSecurityTokenSessionNotificationDisplayed,
                             true);

  std::string domain = GetEnterpriseDomainFromEmail(user_->GetDisplayEmail());
  DisplayNotification(
      l10n_util::GetStringFUTF16(IDS_SECURITY_TOKEN_SESSION_LOCK_MESSAGE_TITLE,
                                 ui::GetChromeOSDeviceName()),
      l10n_util::GetStringFUTF16(IDS_SECURITY_TOKEN_SESSION_LOGOUT_MESSAGE_BODY,
                                 base::UTF8ToUTF16(domain)));
}

void SecurityTokenSessionController::ScheduleLogoutNotification() const {
  // The notification can not be created directly, since it will not persist
  // after the session is ended. Instead, use local state to schedule the
  // creation of a notification.
  if (profile_prefs_->GetBoolean(
          prefs::kSecurityTokenSessionNotificationDisplayed)) {
    // A user should see the notification only the first time they are logged
    // out.
    return;
  }
  profile_prefs_->SetBoolean(prefs::kSecurityTokenSessionNotificationDisplayed,
                             true);
  local_state_->SetString(
      prefs::kSecurityTokenSessionNotificationScheduledDomain,
      GetEnterpriseDomainFromEmail(user_->GetDisplayEmail()));
}

}  // namespace login
}  // namespace chromeos
