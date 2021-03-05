// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_HWID_CHECKER_H_
#define CHROME_BROWSER_ASH_LOGIN_HWID_CHECKER_H_

#include <string>

namespace chromeos {

// Checks if given HWID correct.
bool IsHWIDCorrect(const std::string& hwid);

// Checks if current machine has correct HWID.
bool IsMachineHWIDCorrect();

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_HWID_CHECKER_H_
