// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREENS_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREENS_UTILS_H_

namespace chromeos {
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
void WaitForDiscoverScreen();
void ExitDiscoverPinSetupScreen();
void SkipToEnrollmentOnRecovery();
void WaitForEnrollmentScreen();

void WaitForEulaScreen();
void TapEulaAccept();
void WaitForSyncConsentScreen();
void ExitScreenSyncConsent();

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_OOBE_SCREENS_UTILS_H_
