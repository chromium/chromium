// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_TOUCHPAD_SCROLL_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_TOUCHPAD_SCROLL_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {
class TouchpadScrollScreenView;

// Controller for the Touchpad scroll screen.
class TouchpadScrollScreen : public BaseScreen {
 public:
  using TView = TouchpadScrollScreenView;

  enum class Result { kNext, kNotApplicable };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  TouchpadScrollScreen(base::WeakPtr<TouchpadScrollScreenView> view,
                       const ScreenExitCallback& exit_callback);

  TouchpadScrollScreen(const TouchpadScrollScreen&) = delete;
  TouchpadScrollScreen& operator=(const TouchpadScrollScreen&) = delete;

  ~TouchpadScrollScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  void set_ingore_pref_sync_for_testing(bool ignore_sync) {
    ignore_pref_sync_for_testing_ = ignore_sync;
  }

  static std::string GetResultString(Result result);

 private:
  // BaseScreen:
  bool ShouldBeSkipped(const WizardContext& context) const override;
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  ScreenSummary GetScreenSummary() override;

  // Called when the user changes the toggle button.
  void OnScrollUpdate(bool is_reverse_scroll);

  // Get user synced preferences for touchpad scroll direction.
  bool GetNaturalScrollPrefValue();

  std::string RetrieveChoobeSubtitle();

  bool initial_pref_value_;
  bool ignore_pref_sync_for_testing_ = false;

  base::WeakPtr<TouchpadScrollScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(crbug.com/40163357): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash ::TouchpadScrollScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_TOUCHPAD_SCROLL_SCREEN_H_
