// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_ASH_BROWSER_TEST_STARTER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_ASH_BROWSER_TEST_STARTER_H_

#include "base/files/scoped_temp_dir.h"

namespace ash {

// Helper based on AshBrowserTestStarter, sets up Lacros in a kiosk session.
class KioskAshBrowserTestStarter {
 public:
  // Returns whether the --lacros-chrome-path switch is provided.
  // If it returns false, we should not do any Lacros related testing
  // because the Lacros instance is not provided.
  bool HasLacrosArgument();

  // Must be called in SetUpInProcessBrowserTestFixture.
  void PrepareEnvironmentForKioskLacros();

  // Must be called in SetUpOnMainThread.
  void SetLacrosAvailabilityPolicy();

 private:
  base::ScopedTempDir scoped_temp_dir_xdg_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_KIOSK_ASH_BROWSER_TEST_STARTER_H_
