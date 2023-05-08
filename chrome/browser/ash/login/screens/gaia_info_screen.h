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

  enum class Result { kNext = 0, kBack, kNotApplicable };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  GaiaInfoScreen(base::WeakPtr<GaiaInfoScreenView> view,
                 const ScreenExitCallback& exit_callback);

  GaiaInfoScreen(const GaiaInfoScreen&) = delete;
  GaiaInfoScreen& operator=(const GaiaInfoScreen&) = delete;

  ~GaiaInfoScreen() override;

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<GaiaInfoScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_INFO_SCREEN_H_
