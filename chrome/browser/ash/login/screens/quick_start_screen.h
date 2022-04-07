// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/quick_start_screen_handler.h"

namespace ash {

class QuickStartScreen : public BaseScreen {
 public:
  using TView = QuickStartView;

  enum class Result { CANCEL };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  QuickStartScreen(TView* view, const ScreenExitCallback& exit_callback);

  QuickStartScreen(const QuickStartScreen&) = delete;
  QuickStartScreen& operator=(const QuickStartScreen&) = delete;

  ~QuickStartScreen() override;

  static std::string GetResultString(Result result);

  // This method is called when the view is being destroyed.
  void OnViewDestroyed(TView* view);

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;

  void SendRandomFiguresForTesting() const;

  base::raw_ptr<TView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::QuickStartScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_QUICK_START_SCREEN_H_
