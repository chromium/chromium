// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_WIN_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_WIN_H_

#include "components/password_manager/core/browser/reauth_purpose.h"
#include "ui/gfx/native_widget_types.h"

namespace password_manager_util_win {

// Attempts to (re-)authenticate the user of the OS account. Returns true if
// the user was successfully authenticated, or if authentication was not
// possible.
bool AuthenticateUser(gfx::NativeWindow window,
                      password_manager::ReauthPurpose purpose);
}  // namespace password_manager_util_win

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_UTIL_WIN_H_
