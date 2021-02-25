// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace chromeos {

namespace multidevice_setup {

class MultiDeviceSetupClient;

}  // namespace multidevice_setup

class MultiDeviceSetupScreenView;

class MultiDeviceSetupScreen : public BaseScreen {
 public:
  enum class Result { NEXT, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  MultiDeviceSetupScreen(MultiDeviceSetupScreenView* view,
                         const ScreenExitCallback& exit_callback);
  ~MultiDeviceSetupScreen() override;

  void AddExitCallbackForTesting(const ScreenExitCallback& testing_callback) {
    exit_callback_ = base::BindRepeating(
        [](const ScreenExitCallback& original_callback,
           const ScreenExitCallback& testing_callback, Result result) {
          original_callback.Run(result);
          testing_callback.Run(result);
        },
        exit_callback_, testing_callback);
  }

  void set_multidevice_setup_client_for_testing(
      multidevice_setup::MultiDeviceSetupClient* client) {
    setup_client_ = client;
  }

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const std::string& action_id) override;

 private:
  friend class MultiDeviceSetupScreenTest;

  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/enums.xml, and should always reflect it (do not
  // change one without changing the other).  Entries should be never modified
  // or deleted.  Only additions possible.
  enum class MultiDeviceSetupOOBEUserChoice {
    kAccepted = 0,
    kDeclined = 1,
    kMaxValue = kDeclined
  };

  // Inits `setup_client_` if it was not initialized before.
  void TryInitSetupClient();

  static void RecordMultiDeviceSetupOOBEUserChoiceHistogram(
      MultiDeviceSetupOOBEUserChoice value);

  multidevice_setup::MultiDeviceSetupClient* setup_client_ = nullptr;

  MultiDeviceSetupScreenView* view_;
  ScreenExitCallback exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(MultiDeviceSetupScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MULTIDEVICE_SETUP_SCREEN_H_
