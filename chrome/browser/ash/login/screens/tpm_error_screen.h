// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_TPM_ERROR_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_TPM_ERROR_SCREEN_H_

#include <string>

#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chrome/browser/ui/webui/chromeos/login/tpm_error_screen_handler.h"

namespace ash {

// Controller for the tpm error screen.
class TpmErrorScreen : public BaseScreen {
 public:
  explicit TpmErrorScreen(TpmErrorView* view);
  TpmErrorScreen(const TpmErrorScreen&) = delete;
  TpmErrorScreen& operator=(const TpmErrorScreen&) = delete;
  ~TpmErrorScreen() override;

  // Called when the screen is being destroyed. This should call Unbind() on the
  // associated View if this class is destroyed before that.
  void OnViewDestroyed(TpmErrorView* view);

 private:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserActionDeprecated(const std::string& action_id) override;

  TpmErrorView* view_ = nullptr;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::TpmErrorScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_TPM_ERROR_SCREEN_H_
