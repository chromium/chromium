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

// This enum is used to store the managed Sea Pen policy states for GenAI
// Wallpaper and VC Background, defined in GenAIWallpaperSettings.yaml and
// GenAIVcBackgroundSettings.yaml. Please make sure the enum value and
// definition match with the policy yaml files.
enum class ManagedSeaPenSettings {
  kAllowed = 0,  // Allow [Feature Name] and improve AI models
  kAllowedWithoutLogging =
      1,          // Allow [Feature Name] without improving AI models
  kDisabled = 2,  // Do not allow [Feature Name]
};

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

// Verifies if the current language settings in English.
bool IsSystemInEnglishLanguage();

// Controls whether the profile can see and open SeaPen UI. Managed users have
// age restrictions that underage users (<18) are not allowed to view and access
// SeaPen features. The age check should be verified after network connection
// is established.
bool IsEligibleForSeaPen(Profile* profile);

// Preliminary check whether the profile is allowed to install Sea Pen apps (VC
// Background).
bool IsAllowedToInstallSeaPen(Profile* profile);

// Controls whether SeaPen feature is enabled with the current `settings` value.
bool IsManagedSeaPenSettingsEnabled(const int settings);

// Controls whether SeaPen Wallpaper is enabled for managed profiles.
bool IsManagedSeaPenWallpaperEnabled(Profile* profile);

// Controls whether SeaPen VC Background is enabled for managed profiles.
bool IsManagedSeaPenVcBackgroundEnabled(Profile* profile);

// Controls whether users are eligible for SeaPen text input. The age
// requirements are stricter than for SeaPen.
bool IsEligibleForSeaPenTextInput(Profile* profile);

// Controls whether SeaPen Wallpaper Feedback is shown for managed profiles.
bool IsManagedSeaPenWallpaperFeedbackEnabled(Profile* profile);

// Controls whether SeaPen VC Background Feedback is shown for managed profiles.
bool IsManagedSeaPenVcBackgroundFeedbackEnabled(Profile* profile);

// Return a base64 encoded data url version of `encoded_jpg_data`. The result
// can be displayed directly in a ChromeOS WebUI via img src attribute.
// `encoded_jpg_data` must not be overly large (e.g. bigger than 1k x 1k
// resolution jpg depending on quality) if the result is sent over mojom, or the
// message may be dropped due to size restrictions.
GURL GetJpegDataUrl(std::string_view encoded_jpg_data);

}  // namespace ash::personalization_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_PERSONALIZATION_APP_PERSONALIZATION_APP_UTILS_H_
