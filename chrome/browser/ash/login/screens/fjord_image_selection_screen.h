// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_SELECTION_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"

namespace ash {

class FjordImageSelectionScreenView;

// Implements the OOBE screen for the image selection step.
// This should only be shown in the Fjord variant of OOBE.
class FjordImageSelectionScreen
    : public BaseScreen,
      public screens_common::mojom::FjordImageSelectionPageHandler,
      public OobeMojoBinder<
          screens_common::mojom::FjordImageSelectionPageHandler> {
 public:
  using TView = FjordImageSelectionScreenView;

  enum class Result {
    kCuttlefish = 0,
    kSquid,
  };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  FjordImageSelectionScreen(base::WeakPtr<FjordImageSelectionScreenView> view,
                            const ScreenExitCallback& exit_callback);
  FjordImageSelectionScreen(const FjordImageSelectionScreen&) = delete;
  FjordImageSelectionScreen& operator=(const FjordImageSelectionScreen&) =
      delete;
  ~FjordImageSelectionScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  static std::string GetResultString(Result result);

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  // screens_common::mojom::FjordImageSelectionPageHandler
  void OnImageSelected(
      screens_common::mojom::FjordImageSelectionPageHandler::FjordImageType
          image_type) override;

  ScreenExitCallback exit_callback_;

  base::WeakPtr<FjordImageSelectionScreenView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_SELECTION_SCREEN_H_
