// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MANAGEMENT_TRANSITION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MANAGEMENT_TRANSITION_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "components/prefs/pref_change_registrar.h"

namespace ash {

class ManagementTransitionScreenView;

class ManagementTransitionScreen : public BaseScreen {
 public:
  using TView = class ManagementTransitionScreenView;

  ManagementTransitionScreen(base::WeakPtr<ManagementTransitionScreenView> view,
                             const base::RepeatingClosure& exit_callback);

  ManagementTransitionScreen(const ManagementTransitionScreen&) = delete;
  ManagementTransitionScreen& operator=(const ManagementTransitionScreen&) =
      delete;

  ~ManagementTransitionScreen() override;

  base::OneShotTimer* GetTimerForTesting();

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  base::RepeatingClosure* exit_callback() { return &exit_callback_; }

 private:
  void OnUserAction(const base::Value::List& args) override;
  void OnManagementTransitionFailed();

  // Called when transition has finished, exits the screen.
  void OnManagementTransitionFinished();

  // Whether screen timed out waiting for transition to occur and displayed the
  // error screen.
  bool timed_out_ = false;

  base::TimeTicks screen_shown_time_;

  // Timer used to exit the page when timeout reaches.
  base::OneShotTimer timer_;

  // Listens to pref changes.
  PrefChangeRegistrar registrar_;

  base::WeakPtr<ManagementTransitionScreenView> view_;
  base::RepeatingClosure exit_callback_;

  base::WeakPtrFactory<ManagementTransitionScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MANAGEMENT_TRANSITION_SCREEN_H_
