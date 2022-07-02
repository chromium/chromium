// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREENS_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREENS_UTILS_H_

#include "base/run_loop.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace ash {
namespace test {

// TODO(rsorokin): Move all these functions into a Mixin.
void WaitForWelcomeScreen();
void TapWelcomeNext();
void WaitForNetworkSelectionScreen();
void TapNetworkSelectionNext();
void WaitForUpdateScreen();
void ExitUpdateScreenNoUpdate();
void WaitForFingerprintScreen();
void ExitFingerprintPinSetupScreen();
void WaitForPinSetupScreen();
void ExitPinSetupScreen();
void SkipToEnrollmentOnRecovery();
void WaitForEnrollmentScreen();
void WaitForUserCreationScreen();
void TapUserCreationNext();
// Wait for OobeUI to finish loading.
void WaitForOobeJSReady();

void WaitForEulaScreen();
void TapEulaAccept();
void WaitForSyncConsentScreen();
void ExitScreenSyncConsent();
void WaitForConsolidatedConsentScreen();
void TapConsolidatedConsentAccept();

void ClickSignInFatalScreenActionButton();

bool IsScanningRequestedOnNetworkScreen();
bool IsScanningRequestedOnErrorScreen();

class LanguageReloadObserver : public WelcomeScreen::Observer {
 public:
  explicit LanguageReloadObserver(WelcomeScreen* welcome_screen);
  LanguageReloadObserver(const LanguageReloadObserver&) = delete;
  LanguageReloadObserver& operator==(const LanguageReloadObserver&) = delete;
  ~LanguageReloadObserver() override;

  void Wait();

 private:
  // WelcomeScreen::Observer:
  void OnLanguageListReloaded() override;

  WelcomeScreen* const welcome_screen_;
  base::RunLoop run_loop_;
};

class OobeUiDestroyedWaiter : public OobeUI::Observer {
 public:
  explicit OobeUiDestroyedWaiter(OobeUI*);
  OobeUiDestroyedWaiter(const OobeUiDestroyedWaiter&) = delete;
  ~OobeUiDestroyedWaiter() override;

  void Wait();

 private:
  // OobeUI::Observer
  void OnDestroyingOobeUI() override;
  void OnCurrentScreenChanged(OobeScreenId current_screen,
                              OobeScreenId new_screen) override {}

  bool was_destroyed_ = false;
  base::ScopedObservation<OobeUI, OobeUI::Observer> oobe_ui_observation_{this};
  std::unique_ptr<base::RunLoop> run_loop_;
};

// Use this method when clicking/tapping on something that leads to the
// destruction of the OobeUI. Currently used when clicking on things that
// trigger a device restart/reset.
void TapOnPathAndWaitForOobeToBeDestroyed(
    std::initializer_list<base::StringPiece> element_ids);

}  // namespace test
}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
namespace test {
using ::ash::test::ClickSignInFatalScreenActionButton;
using ::ash::test::ExitFingerprintPinSetupScreen;
using ::ash::test::ExitPinSetupScreen;
using ::ash::test::ExitScreenSyncConsent;
using ::ash::test::ExitUpdateScreenNoUpdate;
using ::ash::test::IsScanningRequestedOnErrorScreen;
using ::ash::test::IsScanningRequestedOnNetworkScreen;
using ::ash::test::TapEulaAccept;
using ::ash::test::TapNetworkSelectionNext;
using ::ash::test::TapUserCreationNext;
using ::ash::test::TapWelcomeNext;
using ::ash::test::WaitForEnrollmentScreen;
using ::ash::test::WaitForEulaScreen;
using ::ash::test::WaitForFingerprintScreen;
using ::ash::test::WaitForNetworkSelectionScreen;
using ::ash::test::WaitForPinSetupScreen;
using ::ash::test::WaitForSyncConsentScreen;
using ::ash::test::WaitForUpdateScreen;
using ::ash::test::WaitForUserCreationScreen;
using ::ash::test::WaitForWelcomeScreen;
}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREENS_UTILS_H_
