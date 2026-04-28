// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_policy_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/reauth_reason.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_notification_delegate.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_policy_utils.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace ash {

namespace {

using AuthFactor = ash::auth::mojom::AuthFactor;

constexpr char kComplexityUpdateNotificationId[] =
    "local_auth_factors_policy_controller.complexity_update";

base::RepeatingClosure& GetOnPrefProcessedClosure() {
  static base::NoDestructor<base::RepeatingClosure> on_pref_processed;
  return *on_pref_processed;
}

base::RepeatingClosure& GetOnNotificationShownClosure() {
  static base::NoDestructor<base::RepeatingClosure> on_notification_shown;
  return *on_notification_shown;
}

ash::auth::mojom::AuthFactorConfig& auth_factor_config(
    PrefService& local_state) {
  return ash::auth::GetAuthFactorConfig(
      quick_unlock::QuickUnlockFactory::GetDelegate(), &local_state);
}

void ShowNotification(Profile* profile,
                      const std::u16string& title,
                      const std::u16string& message,
                      const std::u16string& button_title) {
  message_center::RichNotificationData optional_fields;
  optional_fields.buttons.emplace_back(button_title);
  optional_fields.remove_on_click = false;
  optional_fields.never_timeout = true;

  message_center::NotifierId notifier_id(
      message_center::NotifierType::SYSTEM_COMPONENT,
      kComplexityUpdateNotificationId,
      NotificationCatalogName::kLocalAuthFactorsComplexity);

  auto notification = ash::CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kComplexityUpdateNotificationId,
      title, message, /*display_source=*/std::u16string(),
      /*origin_url=*/GURL(), notifier_id, optional_fields,
      base::MakeRefCounted<LocalAuthFactorsNotificationDelegate>(profile),
      vector_icons::kBusinessIcon,
      message_center::SystemNotificationWarningLevel::WARNING);
  notification.SetSystemPriority();
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, notification, /*metadata=*/nullptr);

  if (auto& callback = GetOnNotificationShownClosure()) {
    callback.Run();
  }
}

}  // namespace

// static
void LocalAuthFactorsPolicyController::SetPrefProcessedCallbackForTesting(
    base::RepeatingClosure on_pref_processed) {
  GetOnPrefProcessedClosure() = std::move(on_pref_processed);
}

// static
void LocalAuthFactorsPolicyController::SetNotificationShownCallbackForTesting(
    base::RepeatingClosure on_notification_shown) {
  GetOnNotificationShownClosure() = std::move(on_notification_shown);
}

PrefService& LocalAuthFactorsPolicyController::prefs() {
  return *pref_change_registrar_.prefs();
}

LocalAuthFactorsPolicyController::LocalAuthFactorsPolicyController(
    PrefService& local_state,
    Profile* profile,
    const AccountId& account_id)
    : profile_(profile), account_id_(account_id) {
  pref_change_registrar_.Init(profile->GetPrefs());
  // `base::Unretained(this)` is safe as `this` outlives the registrar.
  pref_change_registrar_.Add(
      ash::prefs::kAllowedLocalAuthFactors,
      base::BindRepeating(
          &LocalAuthFactorsPolicyController::OnAllowedAuthFactorsPrefUpdated,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ash::prefs::kQuickUnlockModeAllowlist,
      base::BindRepeating(
          &LocalAuthFactorsPolicyController::OnAllowedAuthFactorsPrefUpdated,
          base::Unretained(this)));
  OnAllowedAuthFactorsPrefUpdated();

  auth_factor_config(local_state)
      .ObserveFactorChanges(receiver_.BindNewPipeAndPassRemote());
  pref_change_registrar_.Add(
      ash::prefs::kLocalAuthFactorsComplexity,
      base::BindRepeating(
          &LocalAuthFactorsPolicyController::OnComplexityPrefUpdated,
          base::Unretained(this)));
  OnComplexityPrefUpdated();
}

LocalAuthFactorsPolicyController::~LocalAuthFactorsPolicyController() = default;

void LocalAuthFactorsPolicyController::OnAllowedAuthFactorsPrefUpdated() {
  base::ScopedClosureRunner pref_processed_runner(GetOnPrefProcessedClosure());
  if (!prefs().IsManagedPreference(ash::prefs::kAllowedLocalAuthFactors)) {
    // If the pref is not managed, it means the admin has not set a policy, and
    // thus no action is needed from this handler. Also, it prevents unintended
    // behavior if the preference were to be modified by non-policy means.
    return;
  }

  user_manager::KnownUser known_user(
      user_manager::UserManager::Get()->GetLocalState());
  if (known_user.IsUsingSAML(account_id_)) {
    return;
  }

  auto allowed_auth_factors_set = GetAllowedAuthFactors();

  // Early return in case the policy is not restricting local auth factors.
  if (!allowed_auth_factors_set.has_value() ||
      !allowed_auth_factors_set->empty()) {
    return;
  }

  // We need to determine whether the user has a local knowledge factor setup or
  // not to decide whether to show online reauth on lockscreen.
  auto user_context = std::make_unique<ash::UserContext>();
  user_context->SetAccountId(account_id_);
  GetAuthFactorEditor()->GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(
          &LocalAuthFactorsPolicyController::OnGetAuthFactorsConfiguration,
          weak_factory_.GetWeakPtr(), std::move(pref_processed_runner)));
}

void LocalAuthFactorsPolicyController::OnGetAuthFactorsConfiguration(
    base::ScopedClosureRunner pref_processed_runner,
    std::unique_ptr<ash::UserContext> user_context,
    std::optional<ash::AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors: "
               << error->get_cryptohome_error();
    return;
  }
  CHECK(user_context);
  const auto& config = user_context->GetAuthFactorsConfiguration();
  auto* password_factor =
      config.FindFactorByType(cryptohome::AuthFactorType::kPassword);
  auto* pin_factor = config.FindFactorByType(cryptohome::AuthFactorType::kPin);

  bool pin_is_secondary_and_allowed =
      pin_factor && password_factor &&
      ash::auth::IsGaiaPassword(*password_factor) &&
      !ash::quick_unlock::IsPinDisabledByPolicy(
          pref_change_registrar_.prefs(), ash::quick_unlock::Purpose::kUnlock);

  bool has_local_auth_factors =
      (pin_factor && !pin_is_secondary_and_allowed) ||
      (password_factor && ash::auth::IsLocalPassword(*password_factor));

  if (has_local_auth_factors) {
    user_manager::UserManager::Get()->SaveForceOnlineSignin(
        user_context->GetAccountId(), /*force_online_signin=*/true);
    ash::RecordReauthReason(user_context->GetAccountId(),
                            ash::ReauthReason::kForcedByLocalAuthFactorsPolicy);
  }
  VLOG(1) << "Local auth factors check. Forced online signin: "
          << has_local_auth_factors;
}

AuthFactorEditor* LocalAuthFactorsPolicyController::GetAuthFactorEditor() {
  if (!auth_factor_editor_) {
    auth_factor_editor_ =
        std::make_unique<ash::AuthFactorEditor>(ash::UserDataAuthClient::Get());
  }
  return auth_factor_editor_.get();
}

std::optional<ash::AuthFactorsSet>
LocalAuthFactorsPolicyController::GetAllowedAuthFactors() {
  auto& allowed_auth_factors =
      prefs().GetList(ash::prefs::kAllowedLocalAuthFactors);
  return ash::GetAuthFactorsSetFromPolicyList(&allowed_auth_factors);
}

void LocalAuthFactorsPolicyController::OnFactorChanged(AuthFactor factor) {
  const int enforced_complexity =
      prefs().GetInteger(ash::prefs::kLocalAuthFactorsComplexity);

  switch (factor) {
    case AuthFactor::kPrefBasedPin:
    case AuthFactor::kCryptohomePin:
    case AuthFactor::kCryptohomePinV2:
    case AuthFactor::kLocalPassword:
      // A secret was updated successfully. The new secret naturally complies
      // with the currently enforced policy, so we can mark the current policy
      // as verified.
      // TODO: b/445628211 - Separate verified complexity for PIN and password.
      prefs().SetInteger(ash::prefs::kLocalAuthFactorsVerifiedComplexity,
                         enforced_complexity);
      DismissComplexityUpdateNotification();
      break;
    case AuthFactor::kRecovery:
    case AuthFactor::kGaiaPassword:
      break;
  }
}

void LocalAuthFactorsPolicyController::OnComplexityPrefUpdated() {
  const int enforced_complexity =
      prefs().GetInteger(ash::prefs::kLocalAuthFactorsComplexity);
  const int verified_complexity =
      prefs().GetInteger(ash::prefs::kLocalAuthFactorsVerifiedComplexity);

  // If the policy requires a higher complexity than the user has verified,
  // trigger the notification.
  if (enforced_complexity > verified_complexity) {
    ShowComplexityUpdateNotification();
  } else {
    DismissComplexityUpdateNotification();
  }
}

void LocalAuthFactorsPolicyController::ShowComplexityUpdateNotification() {
  auto user_context = std::make_unique<ash::UserContext>();
  user_context->SetAccountId(account_id_);
  GetAuthFactorEditor()->GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(
          &LocalAuthFactorsPolicyController::OnShowComplexityUpdateNotification,
          weak_factory_.GetWeakPtr()));
}

void LocalAuthFactorsPolicyController::OnShowComplexityUpdateNotification(
    std::unique_ptr<ash::UserContext> user_context,
    std::optional<ash::AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors: "
               << error->get_cryptohome_error();
    return;
  }
  CHECK(user_context);
  const auto& config = user_context->GetAuthFactorsConfiguration();
  auto* password_factor =
      config.FindFactorByType(cryptohome::AuthFactorType::kPassword);
  auto* pin_factor = config.FindFactorByType(cryptohome::AuthFactorType::kPin);

  bool has_password =
      password_factor && ash::auth::IsLocalPassword(*password_factor);
  bool has_pin = !!pin_factor;

  if (!has_password && !has_pin) {
    LOG(WARNING) << "Complexity update required but no local password or PIN "
                    "found for user.";
    return;
  }

  std::u16string title;
  std::u16string message = l10n_util::GetStringUTF16(
      IDS_LOCAL_AUTH_FACTORS_POLICY_COMPLEXITY_UPDATE_MESSAGE);
  std::u16string button_title;

  if (has_password && has_pin) {
    title = l10n_util::GetStringUTF16(
        IDS_LOCAL_AUTH_FACTORS_POLICY_COMPLEXITY_UPDATE_TITLE_BOTH);
    button_title = l10n_util::GetStringUTF16(
        IDS_LOCAL_AUTH_FACTORS_POLICY_COMPLEXITY_UPDATE_BUTTON_BOTH);
  } else if (has_password) {
    title = l10n_util::GetStringUTF16(
        IDS_LOCAL_AUTH_FACTORS_POLICY_COMPLEXITY_UPDATE_TITLE_PASSWORD);
    button_title = l10n_util::GetStringUTF16(
        IDS_LOCAL_AUTH_FACTORS_POLICY_COMPLEXITY_UPDATE_BUTTON_PASSWORD);
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_LOCAL_AUTH_FACTORS_POLICY_COMPLEXITY_UPDATE_TITLE_PIN);
    button_title = l10n_util::GetStringUTF16(
        IDS_LOCAL_AUTH_FACTORS_POLICY_COMPLEXITY_UPDATE_BUTTON_PIN);
  }

  ShowNotification(profile_, title, message, button_title);
}

void LocalAuthFactorsPolicyController::DismissComplexityUpdateNotification() {
  NotificationDisplayServiceFactory::GetForProfile(profile_)->Close(
      NotificationHandler::Type::TRANSIENT, kComplexityUpdateNotificationId);
}

}  // namespace ash
