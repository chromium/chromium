// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_SELECTION_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class FjordImageSelectionScreenView;

// Implements the OOBE screen for the image selection step.
// This should only be shown in the Fjord variant of OOBE.
class FjordImageSelectionScreen : public BaseScreen {
 public:
  using TView = FjordImageSelectionScreenView;

  FjordImageSelectionScreen(base::WeakPtr<FjordImageSelectionScreenView> view,
                            const base::RepeatingClosure& exit_callback);
  FjordImageSelectionScreen(const FjordImageSelectionScreen&) = delete;
  FjordImageSelectionScreen& operator=(const FjordImageSelectionScreen&) =
      delete;
  ~FjordImageSelectionScreen() override;

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  base::RepeatingClosure exit_callback_;

  base::WeakPtr<FjordImageSelectionScreenView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_SELECTION_SCREEN_H_
