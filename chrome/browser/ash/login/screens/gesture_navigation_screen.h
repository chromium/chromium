// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"

namespace ash {

class GestureNavigationScreenView;

// The OOBE screen dedicated to gesture navigation education.
class GestureNavigationScreen
    : public BaseScreen,
      public screens_common::mojom::GestureNavigationPageHandler,
      public OobeMojoBinder<
          screens_common::mojom::GestureNavigationPageHandler> {
 public:
  using TView = GestureNavigationScreenView;
  enum class Result { NEXT, SKIP, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  GestureNavigationScreen(base::WeakPtr<GestureNavigationScreenView> view,
                          const ScreenExitCallback& exit_callback);
  ~GestureNavigationScreen() override;

  GestureNavigationScreen(const GestureNavigationScreen&) = delete;
  GestureNavigationScreen operator=(const GestureNavigationScreen&) = delete;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

 private:
  // Record metrics for the elapsed time that each page was shown for.
  void RecordPageShownTimeMetrics();

  // screens_common::mojom::GestureNavigationPageHandler:
  void OnPageChange(GesturePages page) override;
  void OnSkipClicked() override;
  void OnExitClicked() override;

  base::WeakPtr<GestureNavigationScreenView> view_;
  ScreenExitCallback exit_callback_;

  // Used to keep track of the current elapsed time that each page has been
  // shown for.
  std::map<GesturePages, base::TimeDelta> page_times_;

  // The current page that is shown on the gesture navigation screen.
  GesturePages current_page_;

  // The starting time for the most recently shown page.
  base::TimeTicks start_time_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GESTURE_NAVIGATION_SCREEN_H_
