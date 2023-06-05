// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_INFOBAR_UTILS_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_INFOBAR_UTILS_H_

#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_contents.h"

class Profile;
namespace content {
class WebContents;
}

namespace password_manager {

AccountInfo GetAccountInfoForPasswordMessages(Profile* profile);

std::string GetDisplayableAccountName(content::WebContents* web_contents);

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_INFOBAR_UTILS_H_
