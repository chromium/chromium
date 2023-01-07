// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_
#define CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_

#include "chrome/browser/ash/login/oobe_screen.h"

namespace ash {

// Shows the Chrome OS out-of-box / login UI.
void ShowLoginWizard(OobeScreenId start_screen);

// Shuts down LoginDisplayHostWebUI host and create LoginDisplayHostMojo
// instead.
void SwitchWebUItoMojo();

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_LOGIN_WIZARD_H_
