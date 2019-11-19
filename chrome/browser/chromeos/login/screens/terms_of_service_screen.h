// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"

namespace network {
class SimpleURLLoader;
}

namespace chromeos {

class TermsOfServiceScreenView;

// A screen that shows Terms of Service which have been configured through
// policy. The screen is shown during login and requires the user to accept the
// Terms of Service before proceeding. Currently, Terms of Service are available
// for public sessions only.
class TermsOfServiceScreen : public BaseScreen {
 public:
  enum class Result { ACCEPTED, DECLINED };

  // The possible states that the screen may assume.
  enum class ScreenState : int { LOADING = 0, LOADED = 1, ERROR = 2 };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  TermsOfServiceScreen(TermsOfServiceScreenView* view,
                       const ScreenExitCallback& exit_callback);
  ~TermsOfServiceScreen() override;

  // Called when the user declines the Terms of Service.
  void OnDecline();

  // Called when the user accepts the Terms of Service.
  void OnAccept();

  // Called when view is destroyed so there is no dead reference to it.
  void OnViewDestroyed(TermsOfServiceScreenView* view);

  // BaseScreen:
  void Show() override;
  void Hide() override;

 private:
  // Start downloading the Terms of Service.
  void StartDownload();

  // Abort the attempt to download the Terms of Service if it takes too long.
  void OnDownloadTimeout();

  // Callback function called when SimpleURLLoader completes.
  void OnDownloaded(std::unique_ptr<std::string> response_body);

  TermsOfServiceScreenView* view_;
  ScreenExitCallback exit_callback_;

  std::unique_ptr<network::SimpleURLLoader> terms_of_service_loader_;

  // Timer that enforces a custom (shorter) timeout on the attempt to download
  // the Terms of Service.
  base::OneShotTimer download_timer_;

  DISALLOW_COPY_AND_ASSIGN(TermsOfServiceScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_TERMS_OF_SERVICE_SCREEN_H_
