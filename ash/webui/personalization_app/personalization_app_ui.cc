// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/personalization_app/personalization_app_ui.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ambient/ambient_client.h"
#include "ash/rgb_keyboard/rgb_keyboard_manager.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "ash/webui/common/sea_pen_provider.h"
#include "ash/webui/common/sea_pen_resources.h"
#include "ash/webui/common/sea_pen_resources_generated.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_personalization_app_resources.h"
#include "ash/webui/grit/ash_personalization_app_resources_map.h"
#include "ash/webui/personalization_app/personalization_app_ambient_provider.h"
#include "ash/webui/personalization_app/personalization_app_keyboard_backlight_provider.h"
#include "ash/webui/personalization_app/personalization_app_theme_provider.h"
#include "ash/webui/personalization_app/personalization_app_user_provider.h"
#include "ash/webui/personalization_app/personalization_app_wallpaper_provider.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/cpp/lacros_startup_state.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/manta/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/resources/grit/webui_resources.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::personalization_app {

namespace {

std::u16string GetGooglePhotosURL() {
  return u"https://photos.google.com";
}

bool IsAmbientModeAllowed() {
  return ash::AmbientClient::Get() &&
         ash::AmbientClient::Get()->IsAmbientModeAllowed();
}

AmbientBackendController* GetAmbientBackendController() {
  AmbientBackendController* ambient_backend_controller =
      AmbientBackendController::Get();
  DCHECK(ambient_backend_controller);
  return ambient_backend_controller;
}

void AddResources(content::WebUIDataSource* source) {
  source->AddResourcePath("", IDR_ASH_PERSONALIZATION_APP_INDEX_HTML);
  source->AddResourcePaths(base::make_span(
      kAshPersonalizationAppResources, kAshPersonalizationAppResourcesSize));
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);

#if !DCHECK_IS_ON()
  // Add a default path to avoid crashes when not debugging.
  source->SetDefaultResource(IDR_ASH_PERSONALIZATION_APP_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

void AddStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"personalizationTitle",
       IDS_PERSONALIZATION_APP_PERSONALIZATION_HUB_TITLE},
      {"wallpaperLabel", IDS_PERSONALIZATION_APP_WALLPAPER_LABEL},
      {"defaultWallpaper", IDS_PERSONALIZATION_APP_DEFAULT_WALLPAPER},
      {"back", IDS_PERSONALIZATION_APP_BACK_BUTTON},
      {"currentlySet", IDS_PERSONALIZATION_APP_CURRENTLY_SET},
      {"descriptionDialogOpen",
       IDS_PERSONALIZATION_APP_WALLPAPER_DESCRIPTION_DIALOG_OPEN},
      {"descriptionDialogClose",
       IDS_PERSONALIZATION_APP_WALLPAPER_DESCRIPTION_DIALOG_CLOSE},
      {"myImagesLabel", IDS_PERSONALIZATION_APP_MY_IMAGES},
      {"wallpaperCollections", IDS_PERSONALIZATION_APP_WALLPAPER_COLLECTIONS},
      {"center", IDS_PERSONALIZATION_APP_CENTER},
      {"fill", IDS_PERSONALIZATION_APP_FILL},
      {"changeDaily", IDS_PERSONALIZATION_APP_CHANGE_DAILY},
      {"ariaLabelChangeDaily", IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_DAILY},
      {"refresh", IDS_PERSONALIZATION_APP_REFRESH},
      {"ariaLabelRefresh", IDS_PERSONALIZATION_APP_ARIA_LABEL_REFRESH},
      {"dailyRefresh", IDS_PERSONALIZATION_APP_DAILY_REFRESH},
      {"unknownImageAttribution",
       IDS_PERSONALIZATION_APP_UNKNOWN_IMAGE_ATTRIBUTION},
      {"wallpaperNetworkError",
       IDS_PERSONALIZATION_APP_WALLPAPER_NETWORK_ERROR},
      {"ariaLabelLoading", IDS_PERSONALIZATION_APP_ARIA_LABEL_LOADING},
      // Using old wallpaper app error string pending final revision.
      // TODO(b/195609442)
      {"setWallpaperError", IDS_PERSONALIZATION_APP_SET_WALLPAPER_ERROR},
      {"loadWallpaperError", IDS_PERSONALIZATION_APP_LOAD_WALLPAPER_ERROR},
      {"dismiss", IDS_PERSONALIZATION_APP_DISMISS},
      {"ariaLabelViewFullScreen",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_VIEW_FULL_SCREEN},
      {"exitFullscreen", IDS_PERSONALIZATION_APP_EXIT_FULL_SCREEN},
      {"ariaLabelExitFullscreen",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_EXIT_FULL_SCREEN},
      {"setAsWallpaper", IDS_PERSONALIZATION_APP_SET_AS_WALLPAPER},
      {"zeroImages", IDS_PERSONALIZATION_APP_NO_IMAGES},
      {"oneImage", IDS_PERSONALIZATION_APP_ONE_IMAGE},
      {"multipleImages", IDS_PERSONALIZATION_APP_MULTIPLE_IMAGES},
      {"managedFeature", IDS_PERSONALIZATION_APP_MANAGED_FEATURE},
      {"managedSetting", IDS_PERSONALIZATION_APP_MANAGED_SETTING},
      {"ariaLabelChangeWallpaper",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_WALLPAPER},
      {"ariaLabelHome", IDS_PERSONALIZATION_APP_ARIA_LABEL_HOME},

      // Theme related strings.
      {"themeLabel", IDS_PERSONALIZATION_APP_THEME_LABEL},
      {"dynamicColorLabel", IDS_PERSONALIZATION_APP_THEME_DYNAMIC_COLOR_LABEL},
      {"dynamicColorDescription",
       IDS_PERSONALIZATION_APP_THEME_DYNAMIC_COLOR_DESCRIPTION},
      {"colorSchemeTonalSpot",
       IDS_PERSONALIZATION_APP_THEME_COLOR_SCHEME_TONAL_SPOT},
      {"colorSchemeNeutral",
       IDS_PERSONALIZATION_APP_THEME_COLOR_SCHEME_NEUTRAL},
      {"colorSchemeVibrant",
       IDS_PERSONALIZATION_APP_THEME_COLOR_SCHEME_VIBRANT},
      {"colorSchemeExpressive",
       IDS_PERSONALIZATION_APP_THEME_COLOR_SCHEME_EXPRESSIVE},
      {"staticColorGoogleBlue",
       IDS_PERSONALIZATION_APP_THEME_STATIC_COLOR_GOOGLE_BLUE},
      {"staticColorLightPink",
       IDS_PERSONALIZATION_APP_THEME_STATIC_COLOR_LIGHT_PINK},
      {"staticColorDarkGreen",
       IDS_PERSONALIZATION_APP_THEME_STATIC_COLOR_DARK_GREEN},
      {"staticColorLightPurple",
       IDS_PERSONALIZATION_APP_THEME_STATIC_COLOR_LIGHT_PURPLE},
      {"darkColorMode", IDS_PERSONALIZATION_APP_THEME_DARK_COLOR_MODE},
      {"lightColorMode", IDS_PERSONALIZATION_APP_THEME_LIGHT_COLOR_MODE},
      {"autoColorMode", IDS_PERSONALIZATION_APP_THEME_AUTO_COLOR_MODE},
      {"ariaLabelEnableDarkColorMode",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_ENABLE_DARK_COLOR_MODE},
      {"ariaLabelEnableLightColorMode",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_ENABLE_LIGHT_COLOR_MODE},
      {"ariaLabelEnableAutoColorMode",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_ENABLE_AUTO_COLOR_MODE},
      {"tooltipAutoColorMode", IDS_PERSONALIZATION_APP_TOOLTIP_AUTO_COLOR_MODE},
      {"errorTooltipAutoColorMode",
       IDS_PERSONALIZATION_APP_ERROR_TOOLTIP_AUTO_COLOR_MODE},
      {"managedErrorTooltipAutoColorMode",
       IDS_PERSONALIZATION_APP_MANAGED_ERROR_TOOLTIP_AUTO_COLOR_MODE},
      {"geolocationWarningTextForWeather",
       IDS_PERSONALIZATION_APP_THEME_GEOLOCATION_WARNING_TEXT_FOR_WEATHER},
      {"geolocationWarningManagedTextForWeather",
       IDS_PERSONALIZATION_APP_THEME_GEOLOCATION_WARNING_MANAGED_TEXT_FOR_WEATHER},
      {"autoModeGeolocationDialogText",
       IDS_PERSONALIZATION_APP_THEME_GEOLOCATION_DIALOG_BODY},
      {"autoModeGeolocationDialogConfirmButton",
       IDS_PERSONALIZATION_APP_THEME_GEOLOCATION_DIALOG_CONFIRM_BUTTON},
      {"autoModeGeolocationDialogCancelButton",
       IDS_PERSONALIZATION_APP_THEME_GEOLOCATION_DIALOG_CANCEL_BUTTON},
      {"systemGeolocationDialogTitle",
       IDS_PERSONALIZATION_APP_GEOLOCATION_DIALOG_TITLE},
      {"systemGeolocationDialogBodyParagraph1",
       IDS_PERSONALIZATION_APP_GEOLOCATION_DIALOG_BODY_PARAGRAPH1},
      {"systemGeolocationDialogBodyParagraph2",
       IDS_PERSONALIZATION_APP_GEOLOCATION_DIALOG_BODY_PARAGRAPH2},
      {"systemGeolocationDialogConfirmButton",
       IDS_PERSONALIZATION_APP_GEOLOCATION_DIALOG_CONFIRM_BUTTON},
      {"systemGeolocationDialogCancelButton",
       IDS_PERSONALIZATION_APP_GEOLOCATION_DIALOG_CANCEL_BUTTON},

      // User/avatar related strings.
      {"avatarLabel", IDS_PERSONALIZATION_APP_AVATAR_LABEL},
      {"takeWebcamPhoto", IDS_PERSONALIZATION_APP_AVATAR_TAKE_PHOTO},
      {"takeWebcamVideo", IDS_PERSONALIZATION_APP_AVATAR_TAKE_VIDEO},
      {"webcamCaptureInProgress",
       IDS_PERSONALIZATION_APP_AVATAR_CAPTURE_IN_PROGRESS},
      {"confirmWebcamPhoto", IDS_PERSONALIZATION_APP_AVATAR_CONFIRM_PHOTO},
      {"confirmWebcamVideo", IDS_PERSONALIZATION_APP_AVATAR_CONFIRM_VIDEO},
      {"rejectWebcamPhoto", IDS_PERSONALIZATION_APP_AVATAR_REJECT_PHOTO},
      {"ariaLabelChangeAvatar",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_AVATAR},
      {"ariaLabelGoToAccountSettings",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_GO_TO_ACCOUNT_SETTINGS},
      {"googleProfilePhoto",
       IDS_PERSONALIZATION_APP_AVATAR_GOOGLE_PROFILE_PHOTO},
      {"chooseAFile", IDS_PERSONALIZATION_APP_AVATAR_CHOOSE_A_FILE},
      {"lastExternalImageTitle",
       IDS_PERSONALIZATION_APP_AVATAR_LAST_EXTERNAL_IMAGE},
      {"ariaLabelCurrentAvatar",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_CURRENT_AVATAR},
      {"ariaAnnounceAvatarChanged",
       IDS_PERSONALIZATION_APP_ARIA_ANNOUNCE_AVATAR_CHANGED},
      {"ariaLabelCloseCamera",
       IDS_PERSONALIZATION_APP_AVATAR_ARIA_LABEL_CLOSE_CAMERA},
      {"ariaLabelWebcamVideo",
       IDS_PERSONALIZATION_APP_AVATAR_ARIA_LABEL_WEBCAM_VIDEO},
      {"avatarNetworkError", IDS_PERSONALIZATION_APP_AVATAR_NETWORK_ERROR},

      // Ambient mode related string.
      {"screensaverLabel", IDS_PERSONALIZATION_APP_SCREENSAVER_LABEL},
      {"screenSaverPreviewButton",
       IDS_PERSONALIZATION_APP_SCREENSAVER_PREVIEW_BUTTON},
      {"screenSaverPreviewButtonAriaLabel",
       IDS_PERSONALIZATION_APP_SCREENSAVER_PREVIEW_BUTTON_ARIA_LABEL},
      {"screenSaverPreviewDownloading",
       IDS_PERSONALIZATION_APP_SCREENSAVER_PREVIEW_DOWNLOADING},
      {"screenSaverPreviewDownloadingAriaLabel",
       IDS_PERSONALIZATION_APP_SCREENSAVER_PREVIEW_DOWNLOADING_ARIA_LABEL},
      {"ambientModePageDescription",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_PAGE_DESCRIPTION},
      {"ambientModeOn", IDS_PERSONALIZATION_APP_AMBIENT_MODE_ON},
      {"ambientModeOff", IDS_PERSONALIZATION_APP_AMBIENT_MODE_OFF},
      {"ambientModeDurationTitle",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_DURATION_TITLE},
      {"ambientModeDurationMinutes",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_DURATION_MINUTES},
      {"ambientModeDurationOneHour",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_DURATION_ONE_HOUR},
      {"ambientModeDurationForever",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_DURATION_FOREVER},
      {"ambientModeTopicSourceTitle",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_TITLE},
      {"ambientModeTopicSourceGooglePhotos",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS},
      {"ambientModeTopicSourceGooglePhotosDescription",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS_DESC},
      {"ambientModeTopicSourceGooglePhotosDescriptionNoAlbum",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_GOOGLE_PHOTOS_DESC_NO_ALBUM},
      {"ambientModeTopicSourceArtGallery",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_ART_GALLERY},
      {"ambientModeTopicSourceArtGalleryDescription",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_ART_GALLERY_DESCRIPTION},
      {"ambientModeTopicSourceVideo",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_VIDEO},
      {"ambientModeTopicSourceSelectedRow",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_SELECTED_ROW},
      {"ambientModeTopicSourceUnselectedRow",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_UNSELECTED_ROW},
      {"ambientModeWeatherTitle",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_WEATHER_TITLE},
      {"ambientModeAriaDescriptionWeather",
       IDS_PERSONALIZATION_APP_ARIA_DESCRIPTION_AMBIENT_MODE_WEATHER},
      {"ambientModeWeatherUnitFahrenheit",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_WEATHER_UNIT_FAHRENHEIT},
      {"ambientModeWeatherUnitCelsius",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_WEATHER_UNIT_CELSIUS},
      {"ambientModeAlbumsSubpageRecentHighlightsDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_RECENT_DESC},
      {"ambientModeAlbumsSubpagePhotosNumPluralDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_PHOTOS_NUM_PLURAL},
      {"ambientModeAlbumsSubpagePhotosNumSingularDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_PHOTOS_NUM_SINGULAR},
      {"ambientModeAlbumsSubpageAlbumSelected",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_ALBUM_SELECTED},
      {"ambientModeAlbumsSubpageAlbumUnselected",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_ALBUM_UNSELECTED},
      {"ambientModeLastArtAlbumMessage",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_LAST_ART_ALBUM_MESSAGE},
      {"ambientModeArtAlbumDialogCloseButtonLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ART_ALBUM_DIALOG_CLOSE_BUTTON_LABEL},
      {"ambientModeAnimationTitle",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_TITLE},
      {"ambientModeAnimationSlideshowLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_SLIDESHOW_LABEL},
      {"ambientModeAnimationFeelTheBreezeLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_FEEL_THE_BREEZE_LABEL},
      {"ambientModeAnimationFloatOnByLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_FLOAT_ON_BY_LABEL},
      {"ambientModeAnimationVideoLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ANIMATION_VIDEO_LABEL},
      {"ambientModeZeroStateMessage",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_ZERO_STATE_MESSAGE},
      {"ambientModeMultipleAlbumsDesc",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_MULTIPLE_ALBUMS_DESC},
      {"ambientModeMainPageZeroStateMessage",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_MAIN_PAGE_ZERO_STATE_MESSAGE},
      {"ambientModeMainPageZeroStateMessageV2",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_MAIN_PAGE_ZERO_STATE_MESSAGE_V2},
      {"ambientModeMainPageEnterpriseUserMessage",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_MAIN_PAGE_ENTERPRISE_USER_MESSAGE},
      {"ambientModeTurnOnLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_TURN_ON_LABEL},
      {"ambientModeLearnMoreLabel",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_LEARN_MORE_LABEL},
      {"ariaLabelChangeScreensaver",
       IDS_PERSONALIZATION_APP_ARIA_LABEL_CHANGE_SCREENSAVER},
      {"ambientModeNetworkError",
       IDS_PERSONALIZATION_APP_AMBIENT_MODE_NETWORK_ERROR},

      // Keyboard backlight strings
      {"keyboardBacklightTitle",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_TITLE},
      {"wallpaperColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_WALLPAPER_COLOR_LABEL},
      {"wallpaperColorTooltipText",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_WALLPAPER_COLOR_TOOLTIP_TEXT},
      {"whiteColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_WHITE_COLOR_LABEL},
      {"redColor", IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_RED_COLOR_LABEL},
      {"yellowColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_YELLOW_COLOR_LABEL},
      {"greenColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_GREEN_COLOR_LABEL},
      {"blueColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_BLUE_COLOR_LABEL},
      {"indigoColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_INDIGO_COLOR_LABEL},
      {"purpleColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_PURPLE_COLOR_LABEL},
      {"rainbowColor",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_RAINBOW_COLOR_LABEL},
      {"wallpaperColorNudgeText",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_WALLPAPER_COLOR_NUDGE_TEXT},
      {"zoneCustomize",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_ZONE_CUSTOMIZATION_BUTTON},
      {"dismissButtonText",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_ZONE_CUSTOMIZATION_DISMISS_BUTTON},
      {"wallpaperColorDescription",
       IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_WALLPAPER_COLOR_DESCRIPTION},
      {"zoneTitle", IDS_PERSONALIZATION_APP_KEYBOARD_BACKLIGHT_ZONE_TITLE},
      {"keyboardZonesTitle", IDS_PERSONALIZATION_APP_KEYBOARD_ZONES_TITLE},

      // Google Photos strings
      // TODO(b/229149314): Finalize error and retry strings.
      {"googlePhotosLabel", IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS},
      {"googlePhotosError", IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ERROR},
      {"googlePhotosTryAgain", IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_TRY_AGAIN},
      {"googlePhotosAlbumShared",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ALBUM_SHARED},
      {"googlePhotosSharedAlbumDialogTitle",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_SHARED_ALBUM_DIALOG_TITLE},
      {"googlePhotosSharedAlbumDialogContent",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_SHARED_ALBUM_DIALOG_CONTENT},
      {"googlePhotosSharedAlbumDialogCloseButton",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_SHARED_ALBUM_DIALOG_CLOSE_BUTTON},
      {"googlePhotosSharedAlbumDialogAcceptButton",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_SHARED_ALBUM_DIALOG_ACCEPT_BUTTON},
      {"googlePhotosAlbumsTabLabel",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ALBUMS_TAB},
      {"googlePhotosPhotosTabLabel",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_PHOTOS_TAB},
      {"googlePhotosZeroStateMessage",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ZERO_STATE_MESSAGE},
      {"googlePhotosAlbumZeroStateMessage",
       IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ALBUM_ZERO_STATE_MESSAGE},

      // Time of day Wallpaper/Screen saver strings
      {"timeOfDayBannerDescription",
       IDS_PERSONALIZATION_APP_TIME_OF_DAY_BANNER_DESCRIPTION},
      {"timeOfDayBannerDescriptionNoScreensaver",
       IDS_PERSONALIZATION_APP_TIME_OF_DAY_BANNER_DESCRIPTION_NO_SCREENSAVER},
      {"timeOfDayWallpaperCollectionSublabel",
       IDS_PERSONALIZATION_APP_TIME_OF_DAY_WALLPAPER_COLLECTION_SUBLABEL},
      {"timeOfDayWallpaperDialogTitle",
       IDS_PERSONALIZATION_APP_TIME_OF_DAY_WALLPAPER_DIALOG_TITLE},
      {"timeOfDayWallpaperDialogContent",
       IDS_PERSONALIZATION_APP_TIME_OF_DAY_WALLPAPER_DIALOG_CONTENT},
      {"timeOfDayWallpaperDialogBackButton",
       IDS_PERSONALIZATION_APP_TIME_OF_DAY_WALLPAPER_DIALOG_BACK_BUTTON},
      {"timeOfDayWallpaperDialogConfirmButton",
       IDS_PERSONALIZATION_APP_TIME_OF_DAY_WALLPAPER_DIALOG_CONFIRM_BUTTON}};

  source->AddLocalizedStrings(kLocalizedStrings);

  ::ash::common::AddSeaPenStrings(source);
  ::ash::common::AddSeaPenWallpaperTemplateStrings(source);

  source->AddString("googlePhotosURL", GetGooglePhotosURL());

  source->AddString("timeOfDayWallpaperCollectionId",
                    wallpaper_constants::kTimeOfDayWallpaperCollectionId);

  source->AddString("timeOfDayBannerImageUrl",
                    GetAmbientBackendController()->GetPromoBannerUrl());

  source->AddString("geolocationAccuracyLearnMoreUrl",
                    kPrivacyHubGeolocationAccuracyLearnMoreURL);

  // Product name does not need to be translated.
  auto product_name =
      l10n_util::GetStringUTF16(ui::GetChromeOSDeviceTypeResourceId());
  if (features::IsTimeOfDayScreenSaverEnabled()) {
    product_name = base::UTF8ToUTF16(
        GetAmbientBackendController()->GetTimeOfDayProductName());
  }
  source->AddString(
      "ambientModeTopicSourceVideoDescription",
      l10n_util::GetStringFUTF16(
          IDS_PERSONALIZATION_APP_AMBIENT_MODE_TOPIC_SOURCE_VIDEO_DESCRIPTION,
          product_name));
  source->AddString(
      "timeOfDayBannerTitle",
      l10n_util::GetStringFUTF16(
          IDS_PERSONALIZATION_APP_TIME_OF_DAY_BANNER_TITLE, product_name));

  source->AddString(
      "ambientModeAlbumsSubpageGooglePhotosTitle",
      l10n_util::GetStringFUTF16(
          IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_GOOGLE_PHOTOS_TITLE,
          GetGooglePhotosURL()));
  source->AddString(
      "ambientModeAlbumsSubpageGooglePhotosNoAlbum",
      l10n_util::GetStringFUTF16(
          IDS_PERSONALIZATION_APP_AMBIENT_MODE_ALBUMS_SUBPAGE_GOOGLE_PHOTOS_NO_ALBUM,
          GetGooglePhotosURL()));

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
}

bool ShouldHandleWebUIRequest(const std::string& path) {
  return base::StartsWith(path, "wallpaper.jpg");
}

}  // namespace

PersonalizationAppUI::PersonalizationAppUI(
    content::WebUI* web_ui,
    std::unique_ptr<PersonalizationAppAmbientProvider> ambient_provider,
    std::unique_ptr<PersonalizationAppKeyboardBacklightProvider>
        keyboard_backlight_provider,
    std::unique_ptr<::ash::common::SeaPenProvider> sea_pen_provider,
    std::unique_ptr<PersonalizationAppThemeProvider> theme_provider,
    std::unique_ptr<PersonalizationAppUserProvider> user_provider,
    std::unique_ptr<PersonalizationAppWallpaperProvider> wallpaper_provider)
    : ui::MojoWebUIController(web_ui),
      ambient_provider_(std::move(ambient_provider)),
      keyboard_backlight_provider_(std::move(keyboard_backlight_provider)),
      sea_pen_provider_(std::move(sea_pen_provider)),
      theme_provider_(std::move(theme_provider)),
      user_provider_(std::move(user_provider)),
      wallpaper_provider_(std::move(wallpaper_provider)) {
  start_time_ = base::Time::Now();
  DCHECK(wallpaper_provider_);

  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUIPersonalizationAppHost);

  // Supply a custom wallpaper image.
  source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleWebUIRequest),
      base::BindRepeating(&PersonalizationAppUI::HandleWebUIRequest,
                          weak_ptr_factory_.GetWeakPtr()));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc,
      "worker-src blob: chrome://resources 'self';");

  ash::EnableTrustedTypesCSP(source);

  AddResources(source);
  AddStrings(source);
  AddBooleans(source);
  AddIntegers(source);
}

PersonalizationAppUI::~PersonalizationAppUI() {
  base::TimeDelta duration = base::Time::Now() - start_time_;
  base::UmaHistogramCustomTimes("Ash.Personalization.App.Duration", duration,
                                /*min=*/base::Minutes(1),
                                /*max=*/base::Minutes(30),
                                /*buckets=*/31);
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::AmbientProvider>
        receiver) {
  ambient_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::KeyboardBacklightProvider>
        receiver) {
  keyboard_backlight_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  sea_pen_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::ThemeProvider> receiver) {
  theme_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::WallpaperProvider>
        receiver) {
  wallpaper_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::UserProvider> receiver) {
  user_provider_->BindInterface(std::move(receiver));
}

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void PersonalizationAppUI::AddBooleans(content::WebUIDataSource* source) {
  source->AddBoolean("isGooglePhotosIntegrationEnabled",
                     wallpaper_provider_->IsEligibleForGooglePhotos());

  source->AddBoolean("isGooglePhotosSharedAlbumsEnabled",
                     features::IsWallpaperGooglePhotosSharedAlbumsEnabled());

  source->AddBoolean("isAmbientModeAllowed", IsAmbientModeAllowed());

  source->AddBoolean(
      "isRgbKeyboardSupported",
      Shell::Get()->rgb_keyboard_manager()->IsRgbKeyboardSupported());

  source->AddBoolean("isUserAvatarCustomizationSelectorsEnabled",
                     user_provider_->IsCustomizationSelectorsPrefEnabled());

  source->AddBoolean("isTimeOfDayScreenSaverEnabled",
                     features::IsTimeOfDayScreenSaverEnabled());

  source->AddBoolean("isTimeOfDayWallpaperEnabled",
                     features::IsTimeOfDayWallpaperEnabled());

  source->AddBoolean("isCrosPrivacyHubLocationEnabled",
                     features::IsCrosPrivacyHubLocationEnabled());

  const bool common_sea_pen_requirements =
      sea_pen_provider_->IsEligibleForSeaPen() &&
      manta::features::IsMantaServiceEnabled();
  source->AddBoolean("isSeaPenEnabled",
                     ::ash::features::IsSeaPenEnabled() &&
                         common_sea_pen_requirements);
  source->AddBoolean("isSeaPenTextInputEnabled",
                     common_sea_pen_requirements &&
                         ::ash::features::IsSeaPenTextInputEnabled() &&
                         sea_pen_provider_->IsEligibleForSeaPenTextInput());
  source->AddBoolean("isSeaPenUseExptTemplateEnabled",
                     common_sea_pen_requirements &&
                         ::ash::features::IsSeaPenUseExptTemplateEnabled());
  source->AddBoolean("isManagedSeaPenEnabled",
                     common_sea_pen_requirements &&
                         sea_pen_provider_->IsManagedSeaPenEnabled());
  source->AddBoolean("isManagedSeaPenFeedbackEnabled",
                     sea_pen_provider_->IsManagedSeaPenFeedbackEnabled());
  source->AddBoolean("isLacrosEnabled",
                     ::crosapi::lacros_startup_state::IsLacrosEnabled());
  source->AddBoolean("isVcResizeThumbnailEnabled", false);
}

void PersonalizationAppUI::AddIntegers(content::WebUIDataSource* source) {
  source->AddInteger("keyboardBacklightZoneCount",
                     features::IsMultiZoneRgbKeyboardEnabled()
                         ? Shell::Get()->rgb_keyboard_manager()->GetZoneCount()
                         : 0);
}

void PersonalizationAppUI::HandleWebUIRequest(
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(base::Contains(path, "?key="))
      << "wallpaper key must be provided to prevent browser cache collisions";
  wallpaper_provider_->GetWallpaperAsJpegBytes(std::move(callback));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PersonalizationAppUI)

}  // namespace ash::personalization_app
