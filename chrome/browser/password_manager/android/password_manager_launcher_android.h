// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LAUNCHER_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LAUNCHER_ANDROID_H_

#include "components/password_manager/core/browser/manage_passwords_referrer.h"

namespace content {
class WebContents;
}

namespace password_manager_launcher {

// Opens the password settings page.
void ShowPasswordSettings(content::WebContents* web_contents,
                          password_manager::ManagePasswordsReferrer referrer);

}  // namespace password_manager_launcher

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_MANAGER_LAUNCHER_ANDROID_H_
