// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

class RemoveLocalAuthFactorsScreenView;
// Screen that is shown when the user goes through remove local auth factors
// flow
// TODO: b/445628245 - Implement logic for the screen
class RemoveLocalAuthFactorsScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = RemoveLocalAuthFactorsScreenView;

  enum class Result { kSuccess = 0, kError };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  RemoveLocalAuthFactorsScreen(
      base::WeakPtr<RemoveLocalAuthFactorsScreenView> view,
      const ScreenExitCallback& exit_callback);

  RemoveLocalAuthFactorsScreen(const RemoveLocalAuthFactorsScreen&) = delete;
  RemoveLocalAuthFactorsScreen& operator=(const RemoveLocalAuthFactorsScreen&) =
      delete;

  ~RemoveLocalAuthFactorsScreen() override;

  static std::string GetResultString(Result result);

 private:
  void InspectContext(UserContext* user_context);

  // BaseOSAuthSetupScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::ListValue& args) override;

  base::WeakPtr<RemoveLocalAuthFactorsScreenView> view_;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<RemoveLocalAuthFactorsScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_REMOVE_LOCAL_AUTH_FACTORS_SCREEN_H_
