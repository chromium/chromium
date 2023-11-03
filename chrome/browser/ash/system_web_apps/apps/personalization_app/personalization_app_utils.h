// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_

#include <memory>
#include <string_view>

#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "content/public/browser/web_ui_controller.h"
#include "url/gurl.h"

namespace content {
class WebUI;
}  // namespace content

namespace ash::personalization_app {

// Creates a PersonalizationAppUI. Used as a callback by
// PersonalizationAppUIConfig.
std::unique_ptr<content::WebUIController> CreatePersonalizationAppUI(
    content::WebUI* web_ui,
    const GURL& url);

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

// Return a base64 encoded data url version of `encoded_jpg_data`. The result
// can be displayed directly in a ChromeOS WebUI via img src attribute.
// `encoded_jpg_data` must not be overly large (e.g. bigger than 1k x 1k
// resolution jpg depending on quality) if the result is sent over mojom, or the
// message may be dropped due to size restrictions.
GURL GetJpegDataUrl(std::string_view encoded_jpg_data);

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_
