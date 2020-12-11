// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/offline_login_screen.h"

#include "base/bind.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/offline_login_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/known_user.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace {

constexpr char kUserActionCancel[] = "cancel";

// Amount of time the user has to be idle for before showing the online login
// page.
constexpr const base::TimeDelta kIdleTimeDelta =
    base::TimeDelta::FromMinutes(3);

inline std::string GetEnterpriseDomainManager() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetEnterpriseDomainManager();
}

}  // namespace

// static
std::string OfflineLoginScreen::GetResultString(Result result) {
  switch (result) {
    case Result::BACK:
      return "Back";
    case Result::RELOAD_ONLINE_LOGIN:
      return "ReloadOnlineLogin";
  }
}

OfflineLoginScreen::OfflineLoginScreen(OfflineLoginView* view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(OfflineLoginView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  network_state_informer_ = base::MakeRefCounted<NetworkStateInformer>();
  network_state_informer_->Init();
  if (view_)
    view_->Bind(this);
}

OfflineLoginScreen::~OfflineLoginScreen() {
  if (view_)
    view_->Unbind();
}

void OfflineLoginScreen::OnViewDestroyed(OfflineLoginView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void OfflineLoginScreen::ShowImpl() {
  if (!view_)
    return;
  scoped_observer_ = std::make_unique<base::ScopedObservation<
      NetworkStateInformer, NetworkStateInformerObserver>>(this);
  scoped_observer_->Observe(network_state_informer_.get());
  StartIdleDetection();
  view_->Show();
}

void OfflineLoginScreen::HideImpl() {
  scoped_observer_.reset();
  idle_detector_.reset();
  if (view_)
    view_->Hide();
}

void OfflineLoginScreen::LoadOffline(std::string email) {
  base::DictionaryValue params;

  params.SetString("email", email);
  const std::string enterprise_domain_manager(GetEnterpriseDomainManager());
  if (!enterprise_domain_manager.empty())
    params.SetString("enterpriseDomainManager", enterprise_domain_manager);
  std::string email_domain;
  if (CrosSettings::Get()->GetString(kAccountsPrefLoginScreenDomainAutoComplete,
                                     &email_domain) &&
      !email_domain.empty()) {
    params.SetString("emailDomain", email_domain);
  }
  if (view_)
    view_->LoadParams(params);
}

void OfflineLoginScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionCancel) {
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void OfflineLoginScreen::HandleTryLoadOnlineLogin() {
  exit_callback_.Run(Result::RELOAD_ONLINE_LOGIN);
}

void OfflineLoginScreen::HandleCompleteAuth(const std::string& email,
                                            const std::string& password) {
  const std::string sanitized_email = gaia::SanitizeEmail(email);
  const AccountId account_id = user_manager::known_user::GetAccountId(
      sanitized_email, std::string() /* id */, AccountType::UNKNOWN);
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user) {
    LOG(ERROR) << "OfflineLoginScreen::HandleCompleteAuth: User not found! "
                  "account type="
               << AccountId::AccountTypeToString(account_id.GetAccountType());
    LoginDisplayHost::default_host()->GetLoginDisplay()->ShowError(
        IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, 1,
        HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
    return;
  }

  UserContext user_context(*user);
  user_context.SetKey(Key(password));
  // Save the user's plaintext password for possible authentication to a
  // network. See https://crbug.com/386606 for details.
  user_context.SetPasswordKey(Key(password));
  user_context.SetIsUsingPin(false);
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY) {
    CHECK(user_context.GetUserType() ==
          user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY)
        << "Incorrect Active Directory user type "
        << user_context.GetUserType();
  }
  user_context.SetIsUsingOAuth(false);
  LoginDisplayHost::default_host()->CompleteLogin(user_context);
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
       reason != NetworkError::ERROR_REASON_PORTAL_DETECTED &&
       reason != NetworkError::ERROR_REASON_LOADING_TIMEOUT);
}

}  // namespace chromeos
