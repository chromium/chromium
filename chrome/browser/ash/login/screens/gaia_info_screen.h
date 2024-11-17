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
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_factory.mojom.h"

namespace ash {

class GaiaInfoScreenView;

class GaiaInfoScreen
    : public BaseScreen,
      public screens_common::mojom::GaiaInfoPageHandler,
      public OobeMojoBinder<screens_common::mojom::GaiaInfoPageHandler,
                            screens_common::mojom::GaiaInfoPage> {
 public:
  using TView = GaiaInfoScreenView;

  enum class Result {
    kManual = 0,
    kEnterQuickStart,
    kQuickStartOngoing,
    kBack,
    kNotApplicable
  };

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

  // screens_common::mojom::GaiaInfoPageHandler
  void OnBackClicked() override;
  void OnNextClicked(UserCreationFlowType user_flow) override;

  void SetQuickStartButtonVisibility(bool visible);

  // Whether the QuickStart entry point visibility has already been determined.
  // This flag prevents duplicate histogram entries.
  bool has_emitted_quick_start_visible = false;

  base::WeakPtr<GaiaInfoScreenView> view_;
  ScreenExitCallback exit_callback_;

  // WeakPtrFactory used to schedule other tasks in this object.
  base::WeakPtrFactory<GaiaInfoScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_GAIA_INFO_SCREEN_H_
