// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_SCREEN_H_

#include "chrome/browser/ash/login/screens/base_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ui/webui/chromeos/login/locale_switch_screen_handler.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace chromeos {

// This screen handles OOBE locale switch for the post-login screens.
class LocaleSwitchScreen : public BaseScreen,
                           public signin::IdentityManager::Observer {
 public:
  enum class Result {
    LOCALE_FETCH_FAILED,
    LOCALE_FETCH_TIMEOUT,
    NO_SWITCH_NEEDED,
    SWITCH_SUCCEDED,
    SWITCH_FAILED,
    NOT_APPLICABLE
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  explicit LocaleSwitchScreen(LocaleSwitchView* view,
                              const ScreenExitCallback& exit_callback);
  ~LocaleSwitchScreen() override;

  void OnViewDestroyed(LocaleSwitchView* view);

  // signin::IdentityManager::Observer:
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  bool MaybeSkip(WizardContext* context) override;

  void SwitchLocale(std::string locale);
  void OnLanguageChangedCallback(
      const locale_util::LanguageSwitchResult& result);

  void ResetState();
  void OnTimeout();

  LocaleSwitchView* view_ = nullptr;

  std::string gaia_id_;
  ScreenExitCallback exit_callback_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observer_{this};

  base::OneShotTimer timeout_waiter_;

  base::WeakPtrFactory<LocaleSwitchScreen> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_LOCALE_SWITCH_SCREEN_H_
