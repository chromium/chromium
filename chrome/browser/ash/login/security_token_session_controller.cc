// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/security_token_session_controller.h"

#include <string>
#include <vector>

#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/certificate_provider.h"
#include "chrome/browser/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/security_token_session_restriction_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/login/auth/challenge_response/known_user_pref_utils.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"
#include "chromeos/components/certificate_provider/certificate_info.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
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

namespace ash {
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
  message_center::Notification notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId, title, text,
      /*display_source=*/std::u16string(), /*origin_url=*/GURL(),
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierSecurityTokenSession,
                                 NotificationCatalogName::kSecurityToken),
      /*optional_fields=*/{},
      new message_center::HandleNotificationClickDelegate(
          base::DoNothingAs<void()>()),
      chromeos::kEnterpriseIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
  notification.set_fullscreen_visibility(
      message_center::FullscreenVisibility::OVER_USER);
  notification.SetSystemPriority();
  SystemNotificationHelper::GetInstance()->Display(notification);
}

// Loads the persistently stored information about the challenge-response keys
// that can be used for authenticating the user.
void LoadStoredChallengeResponseSpkiKeysForUser(
    PrefService* local_state,
    const AccountId& account_id,
    base::flat_map<std::string, std::vector<std::string>>* extension_to_spkis,
    base::flat_set<std::string>* extension_ids) {
  // TODO(crbug.com/1164373) This approach does not work for ephemeral users.
  // Instead, only get the certificate that was actually used on the last login.
  const base::Value known_user_value =
      user_manager::KnownUser(local_state).GetChallengeResponseKeys(account_id);
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
  return std::string(spki_bytes);
}

}  // namespace

const char* const
    SecurityTokenSessionController::kNotificationDisplayedKnownUserKey =
        "security_token_session_notification_displayed";

SecurityTokenSessionController::SecurityTokenSessionController(
    bool is_user_profile,
    PrefService* local_state,
    const user_manager::User* primary_user,
    chromeos::CertificateProviderService* certificate_provider_service)
    : is_user_profile_(is_user_profile),
      local_state_(local_state),
      primary_user_(primary_user),
      certificate_provider_service_(certificate_provider_service),
      session_manager_(session_manager::SessionManager::Get()) {
  DCHECK(local_state_);
  DCHECK(primary_user_);
  DCHECK(certificate_provider_service_);
  session_manager_observation_.Observe(session_manager_);
  certificate_provider_ =
      certificate_provider_service_->CreateCertificateProvider();
  LoadStoredChallengeResponseSpkiKeysForUser(
      local_state_, primary_user_->GetAccountId(), &extension_to_spkis_,
      &observed_extensions_);
  UpdateNotificationPref();
  behavior_ = GetBehaviorFromPrefAndSessionState();
  UpdateKeepAlive();
  pref_change_registrar_.Init(local_state_);
  base::RepeatingClosure behavior_pref_changed_callback =
      base::BindRepeating(&SecurityTokenSessionController::UpdateBehavior,
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

void SecurityTokenSessionController::OnChallengeResponseKeysUpdated() {
  extension_to_spkis_.clear();
  observed_extensions_.clear();
  LoadStoredChallengeResponseSpkiKeysForUser(
      local_state_, primary_user_->GetAccountId(), &extension_to_spkis_,
      &observed_extensions_);
}

void SecurityTokenSessionController::OnCertificatesUpdated(
    const std::string& extension_id,
    const std::vector<chromeos::certificate_provider::CertificateInfo>&
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

void SecurityTokenSessionController::OnSessionStateChanged() {
  if (session_manager_->session_state() ==
      session_manager::SessionState::LOCKED) {
    had_lock_screen_transition_ = true;
  }

  // Reset the flag, so that after the certificates are collected from all
  // extensions we know whether the absence of some should be tolerated.
  all_required_certificates_were_observed_ = false;

  UpdateBehavior();
}

// static
void SecurityTokenSessionController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  // Prefs that contain policy values. We use the Local State for these, so that
  // the values are available for the controller regardless of the profile it's
  // attached to (the policy stack has code to automatically copy the primary
  // profile's policies into the Local State).
  registry->RegisterStringPref(prefs::kSecurityTokenSessionBehavior,
                               kIgnorePrefValue);
  registry->RegisterIntegerPref(prefs::kSecurityTokenSessionNotificationSeconds,
                                0);
  // Prefs that contain state that needs to be persisted across Chrome restarts.
  registry->RegisterStringPref(
      prefs::kSecurityTokenSessionNotificationScheduledDomain, "");
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

void SecurityTokenSessionController::UpdateBehavior() {
  Behavior previous_behavior = behavior_;
  behavior_ = GetBehaviorFromPrefAndSessionState();
  UpdateKeepAlive();
  if (behavior_ == Behavior::kIgnore) {
    Reset();
  } else if (previous_behavior == Behavior::kIgnore) {
    // Request all available certificates to ensure that all required
    // certificates are still present.
    certificate_provider_->GetCertificates(base::DoNothing());
  }
}

void SecurityTokenSessionController::UpdateKeepAlive() {
  if (behavior_ == Behavior::kIgnore ||
      crosapi::browser_util::IsAshWebBrowserEnabled()) {
    keep_alive_.reset();
  } else if (!keep_alive_) {
    keep_alive_ = crosapi::BrowserManager::Get()->KeepAlive(
        crosapi::BrowserManager::Feature::kSmartCardSessionController);
  }
}

void SecurityTokenSessionController::UpdateNotificationPref() {
  notification_seconds_ = base::Seconds(local_state_->GetInteger(
      prefs::kSecurityTokenSessionNotificationSeconds));
}

bool SecurityTokenSessionController::ShouldApplyPolicyInCurrentSessionState()
    const {
  switch (session_manager_->session_state()) {
    case session_manager::SessionState::UNKNOWN:
    case session_manager::SessionState::OOBE:
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
    case session_manager::SessionState::LOGIN_SECONDARY:
    case session_manager::SessionState::RMA:
      return false;
    case session_manager::SessionState::ACTIVE:
      if (!is_user_profile_) {
        // Inside the user session, only the controller that's tied to the user
        // profile should work.
        return false;
      }
      return true;
    case session_manager::SessionState::LOCKED:
      if (is_user_profile_) {
        // On the lock screen, only the controller that's tied to the sign-in
        // profile should work.
        return false;
      }
      return true;
  }
  NOTREACHED();
  return false;
}

SecurityTokenSessionController::Behavior
SecurityTokenSessionController::GetBehaviorFromPrefAndSessionState() const {
  // First determine if we're in a session state in which our instance should do
  // nothing (ignore the policy).
  if (!ShouldApplyPolicyInCurrentSessionState())
    return Behavior::kIgnore;
  // After passing the session state checks, use the policy value as the desired
  // behavior.
  return ParseBehaviorPrefValue(
      local_state_->GetString(prefs::kSecurityTokenSessionBehavior));
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
      ScreenLocker::Show();
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
  if (extensions_missing_required_certificates_.empty()) {
    all_required_certificates_were_observed_ = true;
    Reset();
  }
}

void SecurityTokenSessionController::ExtensionStopsProvidingCertificate(
    const extensions::ExtensionId& extension_id) {
  extensions_missing_required_certificates_.insert(extension_id);

  if (!all_required_certificates_were_observed_ &&
      had_lock_screen_transition_) {
    // When transitioning to/from the Lock Screen, we delay applying the policy
    // until we saw the full list of the required certificates at least once.
    // This is needed because the extensions report a spuriously empty list of
    // certificates shortly after such session state transition, due to the USB
    // access conflicts between two profiles.
    return;
  }

  if (fullscreen_notification_) {
    // There was already a security token missing.
    return;
  }

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
                primary_user_->GetDisplayEmail())),
        nullptr, nullptr);
    fullscreen_notification_->Show();
  }
}

void SecurityTokenSessionController::AddLockNotification() {
  // A user should see the notification only the first time their session is
  // locked.
  if (GetNotificationDisplayedKnownUserFlag())
    return;
  SetNotificationDisplayedKnownUserFlag();

  std::string domain = chrome::enterprise_util::GetDomainFromEmail(
      primary_user_->GetDisplayEmail());
  DisplayNotification(
      l10n_util::GetStringFUTF16(IDS_SECURITY_TOKEN_SESSION_LOCK_MESSAGE_TITLE,
                                 ui::GetChromeOSDeviceName()),
      l10n_util::GetStringFUTF16(IDS_SECURITY_TOKEN_SESSION_LOGOUT_MESSAGE_BODY,
                                 base::UTF8ToUTF16(domain)));
}

void SecurityTokenSessionController::ScheduleLogoutNotification() {
  // The notification can not be created directly, since it will not persist
  // after the session is ended. Instead, use local state to schedule the
  // creation of a notification.
  if (GetNotificationDisplayedKnownUserFlag())
    return;
  SetNotificationDisplayedKnownUserFlag();

  local_state_->SetString(
      prefs::kSecurityTokenSessionNotificationScheduledDomain,
      chrome::enterprise_util::GetDomainFromEmail(
          primary_user_->GetDisplayEmail()));
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

bool SecurityTokenSessionController::GetNotificationDisplayedKnownUserFlag()
    const {
  return user_manager::KnownUser(local_state_)
      .FindBoolPath(primary_user_->GetAccountId(),
                    kNotificationDisplayedKnownUserKey)
      .value_or(false);
}

void SecurityTokenSessionController::SetNotificationDisplayedKnownUserFlag() {
  // The reason we use `KnownUser` (i.e., the Local State) here is because the
  // flag needs to be readable/writable from the instance of our class that's
  // tied to the sign-in profile. There's no direct/safe way to access a
  // profile's pref service from a keyed service tied to a different profile.
  user_manager::KnownUser(local_state_)
      .SetBooleanPref(primary_user_->GetAccountId(),
                      kNotificationDisplayedKnownUserKey, true);
}

}  // namespace login
}  // namespace ash
