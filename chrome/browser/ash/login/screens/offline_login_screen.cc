// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/offline_login_screen.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/signin_ui.h"
#include "chrome/browser/ui/webui/ash/login/offline_login_screen_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {

constexpr char kUserActionCancel[] = "cancel";
constexpr char kUserActionEmailSubmitted[] = "email-submitted";
constexpr char kUserActionCompleteAuthentication[] = "complete-authentication";

// Amount of time the user has to be idle for before showing the online login
// page.
constexpr const base::TimeDelta kIdleTimeDelta = base::Minutes(3);

// These values should not be renumbered and numeric values should never
// be reused. This must be kept in sync with ChromeOSHiddenUserPodsOfflineLogin
// in tools/metrics/histograms/enums.xml
enum class OfflineLoginEvent {
  kOfflineLoginEnabled = 0,
  kOfflineLoginBlockedByTimeLimit = 1,
  kOfflineLoginBlockedByInvalidToken = 2,
  kMaxValue = kOfflineLoginBlockedByInvalidToken,
};

inline std::string GetEnterpriseDomainManager() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetEnterpriseDomainManager();
}

void RecordEvent(OfflineLoginEvent event) {
  base::UmaHistogramEnumeration("Login.OfflineLoginWithHiddenUserPods", event);
}

}  // namespace

// static
std::string OfflineLoginScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::BACK:
      return "Back";
    case Result::RELOAD_ONLINE_LOGIN:
      return "ReloadOnlineLogin";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

OfflineLoginScreen::OfflineLoginScreen(base::WeakPtr<OfflineLoginView> view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(OfflineLoginView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
}

OfflineLoginScreen::~OfflineLoginScreen() = default;

void OfflineLoginScreen::ShowImpl() {
  CHECK(session_manager::SessionManager::Get()->session_state() ==
        session_manager::SessionState::LOGIN_PRIMARY);
  if (!view_)
    return;

  scoped_observer_ = std::make_unique<base::ScopedObservation<
      NetworkStateInformer, NetworkStateInformerObserver>>(this);
  scoped_observer_->Observe(network_state_informer_.get());
  StartIdleDetection();

  base::Value::Dict params;
  const std::string enterprise_domain_manager(GetEnterpriseDomainManager());
  if (!enterprise_domain_manager.empty())
    params.Set("enterpriseDomainManager", enterprise_domain_manager);
  std::string email_domain;
  if (CrosSettings::Get()->GetString(kAccountsPrefLoginScreenDomainAutoComplete,
                                     &email_domain) &&
      !email_domain.empty()) {
    params.Set("emailDomain", email_domain);
  }
  view_->Show(std::move(params));
}

void OfflineLoginScreen::HideImpl() {
  scoped_observer_.reset();
  idle_detector_.reset();
  if (view_)
    view_->Hide();
}

void OfflineLoginScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancel) {
    exit_callback_.Run(Result::BACK);
  } else if (action_id == kUserActionEmailSubmitted) {
    CHECK_EQ(args.size(), 2u);
    HandleEmailSubmitted(args[1].GetString());
  } else if (action_id == kUserActionCompleteAuthentication) {
    CHECK_EQ(args.size(), 3u);
    HandleCompleteAuth(args[1].GetString(), args[2].GetString());
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void OfflineLoginScreen::HandleTryLoadOnlineLogin() {
  exit_callback_.Run(Result::RELOAD_ONLINE_LOGIN);
}

void OfflineLoginScreen::HandleCompleteAuth(const std::string& email,
                                            const std::string& password) {
  const std::string sanitized_email = gaia::SanitizeEmail(email);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId account_id = known_user.GetAccountId(
      sanitized_email, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(ERROR) << "OfflineLoginScreen::HandleCompleteAuth: User not found! "
                  "account type="
               << AccountId::AccountTypeToString(account_id.GetAccountType());
    if (!view_)
      return;
    view_->ShowPasswordMismatchMessage();
    return;
  }

  UserContext user_context(*user);
  user_context.SetKey(Key(password));
  user_context.SetLocalPasswordInput(LocalPasswordInput{password});
  // Save the user's plaintext password for possible authentication to a
  // network. See https://crbug.com/386606 for details.
  user_context.SetPasswordKey(Key(password));
  user_context.SetIsUsingPin(false);
  CHECK(account_id.GetAccountType() != AccountType::ACTIVE_DIRECTORY)
      << "Incorrect Active Directory user type " << user_context.GetUserType();
  user_context.SetIsUsingOAuth(false);

  if (ExistingUserController::current_controller()) {
    ExistingUserController::current_controller()->Login(user_context,
                                                        SigninSpecifics());
  } else {
    LOG(ERROR) << "OfflineLoginScreen::HandleCompleteAuth: "
               << "ExistingUserController not available.";
  }
}

void OfflineLoginScreen::HandleEmailSubmitted(const std::string& email) {
  if (!view_)
    return;

  bool offline_limit_expired = false;
  const std::string sanitized_email = gaia::SanitizeEmail(email);
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const AccountId account_id = known_user.GetAccountId(
      sanitized_email, std::string(), AccountType::UNKNOWN);
  const std::optional<base::TimeDelta> offline_signin_interval =
      known_user.GetOfflineSigninLimit(account_id);

  // Further checks only if the limit is set.
  if (offline_signin_interval) {
    const base::Time last_online_signin =
        known_user.GetLastOnlineSignin(account_id);

    offline_limit_expired =
        login::TimeToOnlineSignIn(last_online_signin,
                                  offline_signin_interval.value()) <=
        base::TimeDelta();
  }
  if (offline_limit_expired) {
    RecordEvent(OfflineLoginEvent::kOfflineLoginBlockedByTimeLimit);
    view_->ShowOnlineRequiredDialog();
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (user && user->force_online_signin()) {
    RecordEvent(OfflineLoginEvent::kOfflineLoginBlockedByInvalidToken);
    view_->ShowPasswordPage();
    return;
  }

  RecordEvent(OfflineLoginEvent::kOfflineLoginEnabled);
  view_->ShowPasswordPage();
}

void OfflineLoginScreen::StartIdleDetection() {
  if (!idle_detector_) {
    auto callback = base::BindRepeating(&OfflineLoginScreen::OnIdle,
                                        weak_ptr_factory_.GetWeakPtr());
    idle_detector_ = std::make_unique<IdleDetector>(std::move(callback),
                                                    nullptr /* tick_clock */);
  }
  idle_detector_->Start(kIdleTimeDelta);
}

void OfflineLoginScreen::OnIdle() {
  if (is_network_available_) {
    HandleTryLoadOnlineLogin();
  } else {
    StartIdleDetection();
  }
}

void OfflineLoginScreen::OnNetworkReady() {
  is_network_available_ = true;
}

void OfflineLoginScreen::UpdateState(NetworkError::ErrorReason reason) {
  NetworkStateInformer::State state = network_state_informer_->state();
  is_network_available_ =
      (state == NetworkStateInformer::ONLINE &&
       reason != NetworkError::ERROR_REASON_LOADING_TIMEOUT);
}

void OfflineLoginScreen::ShowPasswordMismatchMessage() {
  if (!view_)
    return;
  view_->ShowPasswordMismatchMessage();
}

}  // namespace ash
