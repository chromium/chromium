// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_TEST_UTIL_H_

namespace chromeos {
enum class HMRConsentStatus;
}  // namespace chromeos

namespace ash {

class MahiManagerImpl;
class MahiUiController;
enum class MahiUiUpdateType;

// Applies the given HMR consent status and waits until the new status becomes
// in effect. NOTE: This function should be called only when the magic boost
// feature is enabled.
void ApplyHMRConsentStatusAndWait(chromeos::HMRConsentStatus status);

// Returns the default Mahi answer used in tests.
const char* GetMahiDefaultTestAnswer();

// Returns the default Mahi summary used in tests.
const char* GetMahiDefaultTestSummary();

// Returns the Mahi UI controller under usage. NOTE: This function should be
// called only if the `MahiManagerImpl` is used.
MahiUiController* GetMahiUiController();

// Waits until the specified `MahiUiUpdate` is received.
void WaitUntilUiUpdateReceived(MahiUiUpdateType target_type);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_TEST_UTIL_H_
