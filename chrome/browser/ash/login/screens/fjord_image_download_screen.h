// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_DOWNLOAD_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_DOWNLOAD_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class FjordImageDownloadScreenView;

// Implements the OOBE screen for showing progress during the image download
// step. This should only be shown in the Fjord variant of OOBE.
class FjordImageDownloadScreen : public BaseScreen {
 public:
  using TView = FjordImageDownloadScreenView;

  FjordImageDownloadScreen(base::WeakPtr<FjordImageDownloadScreenView> view,
                           const base::RepeatingClosure& exit_callback);
  FjordImageDownloadScreen(const FjordImageDownloadScreen&) = delete;
  FjordImageDownloadScreen& operator=(const FjordImageDownloadScreen&) = delete;
  ~FjordImageDownloadScreen() override;

 private:
  // BaseScreen
  void ShowImpl() override;
  void HideImpl() override {}

  base::RepeatingClosure exit_callback_;

  base::WeakPtr<FjordImageDownloadScreenView> view_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_FJORD_IMAGE_DOWNLOAD_SCREEN_H_
