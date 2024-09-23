// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/oobe_mojo_binder.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_common.mojom.h"

namespace ash {

class AppDownloadingScreenView;

// This is App Downloading screen that tells the user the selected Android apps
// are being downloaded.
class AppDownloadingScreen
    : public BaseScreen,
      public screens_common::mojom::AppDownloadingPageHandler,
      public OobeMojoBinder<screens_common::mojom::AppDownloadingPageHandler> {
 public:
  using TView = AppDownloadingScreenView;

  AppDownloadingScreen(base::WeakPtr<TView> view,
                       const base::RepeatingClosure& exit_callback);

  AppDownloadingScreen(const AppDownloadingScreen&) = delete;
  AppDownloadingScreen& operator=(const AppDownloadingScreen&) = delete;

  ~AppDownloadingScreen() override;

  void set_exit_callback_for_testing(base::RepeatingClosure exit_callback) {
    exit_callback_ = exit_callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;

  // screens_common::mojom::AppDownloadingPageHandler
  void OnContinueClicked() override;

 private:
  base::WeakPtr<TView> view_;
  base::RepeatingClosure exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_APP_DOWNLOADING_SCREEN_H_
