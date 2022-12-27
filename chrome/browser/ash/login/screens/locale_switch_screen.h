// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_SCREEN_H_

#include "chrome/browser/ash/login/screens/base_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace ash {

class LocaleSwitchView;

// This screen waits for account information (locale and account capabilities)
// to be fetched and handles OOBE locale switch for the post-login screens.
class LocaleSwitchScreen : public BaseScreen,
                           public signin::IdentityManager::Observer {
 public:
  enum class Result {
    LOCALE_FETCH_FAILED,
    LOCALE_FETCH_TIMEOUT,
    NO_SWITCH_NEEDED,
    SWITCH_SUCCEDED,
    SWITCH_FAILED,
    SWITCH_DELEGATED,
    NOT_APPLICABLE
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  explicit LocaleSwitchScreen(base::WeakPtr<LocaleSwitchView> view,
                              const ScreenExitCallback& exit_callback);
  ~LocaleSwitchScreen() override;

  // signin::IdentityManager::Observer:
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokensLoaded() override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  bool MaybeSkip(WizardContext& context) override;

  void SwitchLocale(std::string locale);
  void OnLanguageChangedCallback(
      const locale_util::LanguageSwitchResult& result);
  void OnLanguageChangedNotificationCallback(
      const locale_util::LanguageSwitchResult& result);

  void ResetState();
  void OnTimeout();

  base::WeakPtr<LocaleSwitchView> view_ = nullptr;

  std::string gaia_id_;
  ScreenExitCallback exit_callback_;
  signin::IdentityManager* identity_manager_ = nullptr;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};

  base::OneShotTimer timeout_waiter_;

  bool refresh_token_loaded_ = false;

  base::WeakPtrFactory<LocaleSwitchScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_SCREEN_H_
