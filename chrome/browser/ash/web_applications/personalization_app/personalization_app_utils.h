// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_

#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash::personalization_app {

// Creates the PersonalizationAppUI to be registered in
// ChromeWebUIControllerFactory.
PersonalizationAppUI* CreatePersonalizationAppUI(content::WebUI* web_ui);

// In general, by the time this function is called, it is already guaranteed
// that there is a valid profile and user that has opened personalization app.
// When calling this function outside of one of the personalization app
// providers, be aware that it may return nullptr.
const user_manager::User* GetUser(const Profile* profile);

// This is also generally called after a user with a regular profile has opened
// personalization app. In the case where this profile has no associated user
// and account id, returns a special |EmptyAccountId| singleton.
AccountId GetAccountId(const Profile* profile);

// Controls whether the profile can see and open personalization app. Most
// profiles can, but kiosk and guest cannot.
bool CanSeeWallpaperOrPersonalizationApp(const Profile* profile);

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_
