// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/security_token_session_controller.h"

#include <string>
#include <vector>

#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/certificate_provider/certificate_info.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/ash/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/security_token_session_restriction_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"
#include "chromeos/login/auth/challenge_response_key.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "extensions/common/extension_id.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
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

void DisplayNotification(const std::u16string& title,
                         const std::u16string& text) {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title,
          text,
          /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
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

// Loads the persistently stored information about the challenge-response keys
// that can be used for authenticating the user.
void LoadStoredChallengeResponseSpkiKeysForUser(
    const AccountId& account_id,
    base::flat_map<std::string, std::vector<std::string>>* extension_to_spkis,
    base::flat_set<std::string>* extension_ids) {
  // TODO(crbug.com/1164373) This approach does not work for ephemeral users.
  // Instead, only get the certificate that was actually used on the last login.
  const base::Value known_user_value =
      user_manager::known_user::GetChallengeResponseKeys(account_id);
  std::vector<DeserializedChallengeResponseKey>
      deserialized_challenge_response_keys;
  DeserializeChallengeResponseKeyFromKnownUser(
      known_user_value, &deserialized_challenge_response_keys);
  for (const DeserializedChallengeResponseKey& challenge_response_key :
       deserialized_challenge_response_keys) {
    if (challenge_response_key.extension_id.empty())
      continue;

    extension_ids->insert(challenge_response_key.extension_id);
    if (!extension_to_spkis->contains(challenge_response_key.extension_id)) {
      (*extension_to_spkis)[challenge_response_key.extension_id] = {};
    }
    if (!challenge_response_key.public_key_spki_der.empty()) {
      (*extension_to_spkis)[challenge_response_key.extension_id].push_back(
          challenge_response_key.public_key_spki_der);
    }
  }
}

std::string GetSubjectPublicKeyInfo(const net::X509Certificate& certificate) {
  base::StringPiece spki_bytes;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer()),
          &spki_bytes)) {
    return {};
  }
  return spki_bytes.as_string();
}

}  // namespace

SecurityTokenSessionController::SecurityTokenSessionController(
    PrefService* local_state,
    PrefService* profile_prefs,
    const user_manager::User* user,
    CertificateProviderService* certificate_provider_service)
    : local_state_(local_state),
      profile_prefs_(profile_prefs),
      user_(user),
      certificate_provider_service_(certificate_provider_service) {
  DCHECK(local_state_);
  DCHECK(profile_prefs_);
  DCHECK(user_);
  DCHECK(certificate_provider_service_);
  certificate_provider_ =
      certificate_provider_service_->CreateCertificateProvider();
  LoadStoredChallengeResponseSpkiKeysForUser(
      user_->GetAccountId(), &extension_to_spkis_, &observed_extensions_);
  UpdateNotificationPref();
  behavior_ = GetBehaviorFromPref();
  pref_change_registrar_.Init(profile_prefs_);
  base::RepeatingClosure behavior_pref_changed_callback =
      base::BindRepeating(&SecurityTokenSessionController::UpdateBehaviorPref,
                          weak_ptr_factory_.GetWeakPtr());
  base::RepeatingClosure notification_pref_changed_callback =
      base::BindRepeating(
          &SecurityTokenSessionController::UpdateNotificationPref,
          weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Add(prefs::kSecurityTokenSessionBehavior,
                             behavior_pref_changed_callback);
  pref_change_registrar_.Add(prefs::kSecurityTokenSessionNotificationSeconds,
                             notification_pref_changed_callback);
  certificate_provider_service_->AddObserver(this);
}

SecurityTokenSessionController::~SecurityTokenSessionController() {
  certificate_provider_service_->RemoveObserver(this);
}

void SecurityTokenSessionController::Shutdown() {
  pref_change_registrar_.RemoveAll();
}

void SecurityTokenSessionController::OnCertificatesUpdated(
    const std::string& extension_id,
    const std::vector<certificate_provider::CertificateInfo>&
        certificate_infos) {
  if (behavior_ == Behavior::kIgnore)
    return;

  if (!observed_extensions_.contains(extension_id))
    return;

  if (extension_to_spkis_[extension_id].empty())
    return;

  bool extension_provides_all_required_certificates = true;

  std::vector<std::string> provided_spki_vector;
  for (auto certificate_info : certificate_infos) {
    provided_spki_vector.emplace_back(
        GetSubjectPublicKeyInfo(*certificate_info.certificate.get()));
  }
  base::flat_set<std::string> provided_spkis(provided_spki_vector.begin(),
                                             provided_spki_vector.end());
  auto& expected_spkis = extension_to_spkis_[extension_id];
  for (const auto& expected_spki : expected_spkis) {
    if (!provided_spkis.contains(expected_spki)) {
      extension_provides_all_required_certificates = false;
      break;
    }
  }

  if (extension_provides_all_required_certificates) {
    ExtensionProvidesAllRequiredCertificates(extension_id);
  } else {
    ExtensionStopsProvidingCertificate(extension_id);
  }
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
  // Sanitize `scheduled_notification_domain`, as values coming from local state
  // are not trusted.
  std::string domain = scheduled_notification_domain->GetValue()->GetString();
  local_state->ClearPref(
      prefs::kSecurityTokenSessionNotificationScheduledDomain);
  std::string sanitized_domain;
  if (!SanitizeDomain(domain, sanitized_domain)) {
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
  Behavior previous_behavior = behavior_;
  behavior_ = GetBehaviorFromPref();
  if (behavior_ == Behavior::kIgnore) {
    Reset();
  } else if (previous_behavior == Behavior::kIgnore) {
    // Request all available certificates to ensure that all required
    // certificates are still present.
    certificate_provider_->GetCertificates(base::DoNothing());
  }
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

void SecurityTokenSessionController::TriggerAction() {
  if (fullscreen_notification_ && !fullscreen_notification_->IsClosed()) {
    fullscreen_notification_->CloseWithReason(
        views::Widget::ClosedReason::kAcceptButtonClicked);
  }
  Reset();
  switch (behavior_) {
    case Behavior::kIgnore:
      return;
    case Behavior::kLock:
      chromeos::ScreenLocker::Show();
      AddLockNotification();
      return;
    case Behavior::kLogout:
      chrome::AttemptExit();
      ScheduleLogoutNotification();
      return;
  }
  NOTREACHED();
}

void SecurityTokenSessionController::ExtensionProvidesAllRequiredCertificates(
    const extensions::ExtensionId& extension_id) {
  extensions_missing_required_certificates_.erase(extension_id);
  if (extensions_missing_required_certificates_.empty())
    Reset();
}

void SecurityTokenSessionController::ExtensionStopsProvidingCertificate(
    const extensions::ExtensionId& extension_id) {
  extensions_missing_required_certificates_.insert(extension_id);

  if (fullscreen_notification_)
    // There was already a security token missing.
    return;

  // Schedule session lock / logout.
  action_timer_.Start(
      FROM_HERE, notification_seconds_,
      base::BindOnce(&SecurityTokenSessionController::TriggerAction,
                     weak_ptr_factory_.GetWeakPtr()));

  if (!notification_seconds_.is_zero()) {
    fullscreen_notification_ = views::DialogDelegate::CreateDialogWidget(
        std::make_unique<SecurityTokenSessionRestrictionView>(
            notification_seconds_,
            base::BindOnce(&SecurityTokenSessionController::TriggerAction,
                           weak_ptr_factory_.GetWeakPtr()),
            behavior_,
            chrome::enterprise_util::GetDomainFromEmail(
                user_->GetDisplayEmail())),
        nullptr, nullptr);
    fullscreen_notification_->Show();
  }
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

  std::string domain =
      chrome::enterprise_util::GetDomainFromEmail(user_->GetDisplayEmail());
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
      chrome::enterprise_util::GetDomainFromEmail(user_->GetDisplayEmail()));
}

void SecurityTokenSessionController::Reset() {
  action_timer_.Stop();
  extensions_missing_required_certificates_.clear();
  if (fullscreen_notification_) {
    if (!fullscreen_notification_->IsClosed()) {
      fullscreen_notification_->CloseWithReason(
          views::Widget::ClosedReason::kEscKeyPressed);
    }
    fullscreen_notification_ = nullptr;
  }
}

}  // namespace login
}  // namespace chromeos
