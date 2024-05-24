// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_AUTO_SIGNIN_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_AUTO_SIGNIN_PROMPT_CONTROLLER_H_

#include <string>

namespace content {
class WebContents;
}

// Shows an auto sign-in prompt in order to inform the users that they were
// automatically signed in to the website. |username| is the username used by
// the user in order to login to the web site, it can be email, telephone number
// or any string.
void ShowAutoSigninPrompt(content::WebContents* web_contents,
                          const std::u16string& username);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_AUTO_SIGNIN_PROMPT_CONTROLLER_H_
