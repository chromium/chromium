// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_GEMINI_INTRO_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_GEMINI_INTRO_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"

namespace ash {

class GeminiIntroScreenView;

class GeminiIntroScreen
    : public BaseScreen,
      public screens_common::mojom::GeminiIntroPageHandler,
      public OobeMojoBinder<screens_common::mojom::GeminiIntroPageHandler> {
 public:
  using TView = GeminiIntroScreenView;

  enum class Result {
    kNext = 0,
    kBack,
    kNotApplicable
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  GeminiIntroScreen(base::WeakPtr<GeminiIntroScreenView> view,
              const ScreenExitCallback& exit_callback);

  GeminiIntroScreen(const GeminiIntroScreen&) = delete;
  GeminiIntroScreen& operator=(const GeminiIntroScreen&) = delete;

  ~GeminiIntroScreen() override;

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
  // screens_common::mojom::GeminiIntroPageHandler
  void OnBackClicked() override;
  void OnNextClicked() override;

  base::WeakPtr<GeminiIntroScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GEMINI_INTRO_SCREEN_H_
