// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_AI_INTRO_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_AI_INTRO_SCREEN_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"

namespace ash {

class AiIntroScreenView;
struct AccessibilityStatusEventDetails;

class AiIntroScreen
    : public BaseScreen,
      public screens_common::mojom::AiIntroPageHandler,
      public OobeMojoBinder<screens_common::mojom::AiIntroPageHandler,
                            screens_common::mojom::AiIntroPage> {
 public:
  using TView = AiIntroScreenView;

  enum class Result {
    kNext = 0,
    kNotApplicable
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  AiIntroScreen(base::WeakPtr<AiIntroScreenView> view,
                const ScreenExitCallback& exit_callback);

  AiIntroScreen(const AiIntroScreen&) = delete;
  AiIntroScreen& operator=(const AiIntroScreen&) = delete;

  ~AiIntroScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  static std::string GetResultString(Result result);
  static bool ShouldBeSkipped();

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;

  // Notification of a change in the accessibility settings.
  // Pause autoscroll in oobe_carousel if cvox is active.
  void OnAccessibilityStatusChanged(
      const AccessibilityStatusEventDetails& details);

  // screens_common::mojom::AiIntroPageHandler:
  void OnNextClicked() override;

  base::CallbackListSubscription accessibility_subscription_;
  base::WeakPtr<AiIntroScreenView> view_;
  ScreenExitCallback exit_callback_;

  // WeakPtrFactory used to schedule other tasks in this object.
  base::WeakPtrFactory<AiIntroScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_AI_INTRO_SCREEN_H_
