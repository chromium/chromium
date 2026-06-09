// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_EXTENSIONS_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_EXTENSIONS_H_

#include <string>

#include "base/auto_reset.h"
#include "build/buildflag.h"
#include "components/signin/public/base/signin_buildflags.h"

class Profile;

namespace signin_ui_util {
class SigninUiDelegate;
}

// Delegates to an existing sign-in tab if one exists. If not, a new sign-in tab
// is created.
void ShowExtensionSigninPrompt(Profile* profile,
                               bool enable_sync,
                               const std::string& email_hint);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
base::AutoReset<signin_ui_util::SigninUiDelegate*>
SetSigninUiDelegateForExtensionsTesting(
    signin_ui_util::SigninUiDelegate* delegate);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UI_UTIL_EXTENSIONS_H_
