// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREENS_UTILS_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREENS_UTILS_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"

namespace ash {
namespace test {

// TODO(rsorokin): Move all these functions into a Mixin.
void WaitForWelcomeScreen();
void TapWelcomeNext();
void WaitForNetworkSelectionScreen();
void TapNetworkSelectionNext();
void WaitForUpdateScreen();
void ExitUpdateScreenNoUpdate();
void WaitForConsumerUpdateScreen();
void ExitConsumerUpdateScreenNoUpdate();
void WaitForFingerprintScreen();
void ExitFingerprintPinSetupScreen();
void WaitForPinSetupScreen();
void ExitPinSetupScreen();
void SkipToEnrollmentOnRecovery();
void WaitForEnrollmentScreen();
void WaitForUserCreationScreen();
void TapForPersonalUseCrRadioButton();
void TapUserCreationNext();
void WaitForGaiaInfoScreen();
// Wait for OobeUI to finish loading.
void WaitForOobeJSReady();

void WaitForSyncConsentScreen();
void ExitScreenSyncConsent();
void WaitForConsolidatedConsentScreen();
void TapConsolidatedConsentAccept();
void WaitForGuestTosScreen();
void TapGuestTosAccept();

void ClickSignInFatalScreenActionButton();

bool IsScanningRequestedOnNetworkScreen();
bool IsScanningRequestedOnErrorScreen();

void SetFakeTouchpadDevice();

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

  const raw_ptr<WelcomeScreen> welcome_screen_;
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
    std::initializer_list<std::string_view> element_ids);

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_SCREENS_UTILS_H_
