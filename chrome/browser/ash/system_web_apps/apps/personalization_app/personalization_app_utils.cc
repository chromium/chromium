// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"

#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_ambient_provider_impl.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_keyboard_backlight_provider_impl.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_theme_provider_impl.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_user_provider_impl.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_wallpaper_provider_impl.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/language/core/common/locale_util.h"
#include "components/manta/manta_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "url/gurl.h"

namespace ash::personalization_app {

namespace {
bool CanAccessMantaFeaturesWithoutMinorRestrictions(Profile* profile) {
  // Only users who can access manta features without minor restrictions will
  // have SeaPen enabled.
  auto* manta_service = manta::MantaServiceFactory::GetForProfile(profile);
  const bool canAccessMantaFeaturesWithoutMinorRestrictions =
      manta_service &&
      manta_service->CanAccessMantaFeaturesWithoutMinorRestrictions() ==
          manta::FeatureSupportStatus::kSupported;
  return canAccessMantaFeaturesWithoutMinorRestrictions;
}
}  // namespace

std::unique_ptr<content::WebUIController> CreatePersonalizationAppUI(
    content::WebUI* web_ui,
    const GURL& url) {
  auto ambient_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppAmbientProviderImpl>(web_ui);
  auto keyboard_backlight_provider =
      std::make_unique<ash::personalization_app::
                           PersonalizationAppKeyboardBacklightProviderImpl>(
          web_ui);
  auto theme_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppThemeProviderImpl>(web_ui);
  auto user_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppUserProviderImpl>(web_ui);
  auto wallpaper_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppWallpaperProviderImpl>(
      web_ui,
      std::make_unique<wallpaper_handlers::WallpaperFetcherDelegateImpl>());
  auto sea_pen_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppSeaPenProviderImpl>(
      web_ui,
      std::make_unique<wallpaper_handlers::WallpaperFetcherDelegateImpl>());
  return std::make_unique<ash::personalization_app::PersonalizationAppUI>(
      web_ui, std::move(ambient_provider),
      std::move(keyboard_backlight_provider), std::move(sea_pen_provider),
      std::move(theme_provider), std::move(user_provider),
      std::move(wallpaper_provider));
}

const user_manager::User* GetUser(const Profile* profile) {
  auto* profile_helper = ProfileHelper::Get();
  DCHECK(profile_helper);
  const user_manager::User* user = profile_helper->GetUserByProfile(profile);
  return user;
}

AccountId GetAccountId(const Profile* profile) {
  const auto* user = GetUser(profile);
  if (!user) {
    return EmptyAccountId();
  }
  return user->GetAccountId();
}

bool CanSeeWallpaperOrPersonalizationApp(const Profile* profile) {
  const auto* user = GetUser(profile);
  if (!user) {
    return false;
  }
  switch (user->GetType()) {
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return false;
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
    case user_manager::UserType::kGuest:
    // Public account users must be able to see personalization app since retail
    // demo mode is implemented as a public account.
    case user_manager::UserType::kPublicAccount:
      return true;
  }
}

bool IsSystemInEnglishLanguage() {
  return g_browser_process != nullptr &&
         language::ExtractBaseLanguage(
             g_browser_process->GetApplicationLocale()) == "en";
}

bool IsEligibleForSeaPen(Profile* profile) {
  if (!IsAllowedToInstallSeaPen(profile)) {
    return false;
  }

  if (!profile->GetProfilePolicyConnector()->IsManaged()) {
    return true;
  }

  // TODO(b/365134596): remove the exception for Googlers and Demo Mode once the
  // SeaPenEnterprise flag is removed.
  if (gaia::IsGoogleInternalAccountEmail(profile->GetProfileUserName())) {
    DVLOG(1) << __func__ << " Google internal account";
    return true;
  }

  if (features::IsSeaPenDemoModeEnabled() &&
      DemoSession::IsDeviceInDemoMode()) {
    DVLOG(1) << __func__ << " demo mode";
    const auto* user = GetUser(profile);
    return DemoSession::Get() && user &&
           user->GetType() == user_manager::UserType::kPublicAccount;
  }

  if (!features::IsSeaPenEnterpriseEnabled()) {
    // Without the experiment, managed users are not allowed for SeaPen.
    DVLOG(1) << __func__ << " managed profile";
    return false;
  }

  return CanAccessMantaFeaturesWithoutMinorRestrictions(profile);
}

bool IsAllowedToInstallSeaPen(Profile* profile) {
  if (!profile) {
    LOG(ERROR) << __func__ << " no profile";
    return false;
  }

  // Show for Googlers.
  if (gaia::IsGoogleInternalAccountEmail(profile->GetProfileUserName())) {
    DVLOG(1) << __func__ << " Google internal account";
    return true;
  }

  if (features::IsSeaPenDemoModeEnabled() &&
      DemoSession::IsDeviceInDemoMode()) {
    DVLOG(1) << __func__ << " demo mode";
    const auto* user = GetUser(profile);
    return DemoSession::Get() && user &&
           user->GetType() == user_manager::UserType::kPublicAccount;
  }

  const auto* user = GetUser(profile);
  if (!user) {
    LOG(ERROR) << __func__ << " no user";
    return false;
  }
  DVLOG(1) << __func__ << " user_type=" << user->GetType();
  switch (user->GetType()) {
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
    case user_manager::UserType::kChild:
    // Demo mode retail devices are type kPublicAccount and may have been
    // handled earlier in this function. But not all kPublicAccount users are in
    // demo mode. Public ChromeOS devices at libraries etc often use
    // kPublicAccount type.
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kGuest:
      return false;
    case user_manager::UserType::kRegular:
      if (profile->GetProfilePolicyConnector()->IsManaged()) {
        // Without the experiment, managed users are not allowed for SeaPen.
        DVLOG(1) << __func__ << " managed profile";
        return features::IsSeaPenEnterpriseEnabled();
      }
      return true;
  }
}

bool IsManagedSeaPenSettingsEnabled(const int settings) {
  switch (static_cast<ManagedSeaPenSettings>(settings)) {
    case ManagedSeaPenSettings::kAllowed:
    case ManagedSeaPenSettings::kAllowedWithoutLogging:
      return true;
    case ManagedSeaPenSettings::kDisabled:
    default:
      return false;
  }
}

bool IsManagedSeaPenWallpaperEnabled(Profile* profile) {
  return IsManagedSeaPenSettingsEnabled(
      profile->GetPrefs()->GetInteger(ash::prefs::kGenAIWallpaperSettings));
}

bool IsManagedSeaPenWallpaperFeedbackEnabled(Profile* profile) {
  return profile->GetPrefs()->GetInteger(ash::prefs::kGenAIWallpaperSettings) ==
         static_cast<int>(ManagedSeaPenSettings::kAllowed);
}

bool IsManagedSeaPenVcBackgroundEnabled(Profile* profile) {
  return IsManagedSeaPenSettingsEnabled(
      profile->GetPrefs()->GetInteger(ash::prefs::kGenAIVcBackgroundSettings));
}

bool IsManagedSeaPenVcBackgroundFeedbackEnabled(Profile* profile) {
  return profile->GetPrefs()->GetInteger(
             ash::prefs::kGenAIVcBackgroundSettings) ==
         static_cast<int>(ManagedSeaPenSettings::kAllowed);
}

bool IsEligibleForSeaPenTextInput(Profile* profile) {
  if (!profile) {
    LOG(ERROR) << __func__ << " no profile";
    return false;
  }
  if (!features::IsSeaPenTextInputEnabled()) {
    // Without the experiment, users are not allowed to use SeaPenTextInput.
    DVLOG(1) << __func__ << " SeaPenTextInput disabled";
    return false;
  }
  if (!IsSystemInEnglishLanguage()) {
    // The feature only supports English users.
    DVLOG(1) << __func__ << " system not in English language";
    return false;
  }
  return IsEligibleForSeaPen(profile) &&
         CanAccessMantaFeaturesWithoutMinorRestrictions(profile);
}

GURL GetJpegDataUrl(std::string_view encoded_jpg_data) {
  return GURL(base::StrCat(
      {"data:image/jpeg;base64,", base::Base64Encode(encoded_jpg_data)}));
}

}  // namespace ash::personalization_app
