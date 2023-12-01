// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_INFO_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_INFO_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class GaiaInfoScreenView;

class GaiaInfoScreen : public BaseScreen {
 public:
  using TView = GaiaInfoScreenView;

  enum class Result { kManual = 0, kQuickstart, kBack, kNotApplicable };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  GaiaInfoScreen(base::WeakPtr<GaiaInfoScreenView> view,
                 const ScreenExitCallback& exit_callback);

  GaiaInfoScreen(const GaiaInfoScreen&) = delete;
  GaiaInfoScreen& operator=(const GaiaInfoScreen&) = delete;

  ~GaiaInfoScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void SetQuickStartButtonVisibility(bool visible);

  base::WeakPtr<GaiaInfoScreenView> view_;
  ScreenExitCallback exit_callback_;

  // WeakPtrFactory used to schedule other tasks in this object.
  base::WeakPtrFactory<GaiaInfoScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_INFO_SCREEN_H_
