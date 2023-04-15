// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_ASH_ACCELERATOR_HELPERS_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_ASH_ACCELERATOR_HELPERS_H_

namespace ash {

// Presses Ctrl + Shift + Q twice using the system shell as accelerator target.
[[nodiscard]] bool PressSignOutAccelerator();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_TEST_ASH_ACCELERATOR_HELPERS_H_
