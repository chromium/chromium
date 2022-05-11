// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_

#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {
namespace personalization_app {

// Creates the PersonalizationAppUI to be registered in
// ChromeWebUIControllerFactory.
PersonalizationAppUI* CreatePersonalizationAppUI(content::WebUI* web_ui);

const user_manager::User* GetUser(const Profile* profile);

AccountId GetAccountId(const Profile* profile);

}  // namespace personalization_app
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_
