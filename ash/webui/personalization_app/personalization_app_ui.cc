// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/personalization_app_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_personalization_app_resources.h"
#include "ash/grit/ash_personalization_app_resources_map.h"
#include "ash/webui/personalization_app/personalization_app_ui_delegate.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "base/strings/strcat.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash {

namespace {

bool ShouldIncludeResource(const webui::ResourcePath& resource) {
  return base::StartsWith(resource.path, "trusted") ||
         base::StartsWith(resource.path, "common") ||
         resource.id == IDR_ASH_PERSONALIZATION_APP_ICON_192_PNG;
}

void AddResources(content::WebUIDataSource* source) {
  source->AddResourcePath("", IDR_ASH_PERSONALIZATION_APP_TRUSTED_INDEX_HTML);

  const auto resources = base::make_span(kAshPersonalizationAppResources,
                                         kAshPersonalizationAppResourcesSize);

  for (const auto& resource : resources) {
    if (ShouldIncludeResource(resource))
      source->AddResourcePath(resource.path, resource.id);
  }
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML);
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);

#if !DCHECK_IS_ON()
  // Add a default path to avoid crash when not debugging.
  source->SetDefaultResource(IDR_ASH_PERSONALIZATION_APP_TRUSTED_INDEX_HTML);
#endif  // !DCHECK_IS_ON()
}

void AddStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"title", IDS_PERSONALIZATION_APP_TITLE},
      {"back", IDS_PERSONALIZATION_APP_BACK_BUTTON},
      {"currentlySet", IDS_PERSONALIZATION_APP_CURRENTLY_SET},
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
      {"networkError", IDS_PERSONALIZATION_APP_NETWORK_ERROR},
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
      {"setAsWallpaper", IDS_PERSONALIZATION_APP_SET_AS_WALLPAPER}};
  source->AddLocalizedStrings(kLocalizedStrings);

  if (features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    static constexpr webui::LocalizedString kGooglePhotosLocalizedStrings[] = {
        {"googlePhotosLabel", IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS},
        {"googlePhotosAlbumsTabLabel",
         IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ALBUMS_TAB},
        {"googlePhotosPhotosTabLabel",
         IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_PHOTOS_TAB},
        {"googlePhotosZeroStateMessage",
         IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS_ZERO_STATE_MESSAGE}};
    source->AddLocalizedStrings(kGooglePhotosLocalizedStrings);
  }

  source->UseStringsJs();
}

void AddBooleans(content::WebUIDataSource* source) {
  source->AddBoolean("fullScreenPreviewEnabled",
                     features::IsWallpaperFullScreenPreviewEnabled());

  source->AddBoolean("isGooglePhotosIntegrationEnabled",
                     features::IsWallpaperGooglePhotosIntegrationEnabled());

  source->AddBoolean("isPersonalizationHubEnabled",
                     features::IsPersonalizationHubEnabled());

  source->AddBoolean("isDarkLightModeEnabled",
                     features::IsDarkLightModeEnabled());
}

}  // namespace

PersonalizationAppUI::PersonalizationAppUI(
    content::WebUI* web_ui,
    std::unique_ptr<PersonalizationAppUiDelegate> delegate)
    : ui::MojoWebUIController(web_ui), delegate_(std::move(delegate)) {
  DCHECK(delegate_);

  std::unique_ptr<content::WebUIDataSource> source = base::WrapUnique(
      content::WebUIDataSource::Create(kChromeUIPersonalizationAppHost));

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");

  // Allow requesting a chrome-untrusted://personalization/ iframe.
  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      base::StrCat(
          {"frame-src ", kChromeUIUntrustedPersonalizationAppURL, ";"}));

  // TODO(crbug/1169829) set up trusted types properly to allow Polymer to write
  // html
  source->DisableTrustedTypesCSP();

  AddResources(source.get());
  AddStrings(source.get());
  AddBooleans(source.get());

  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source.release());
}

PersonalizationAppUI::~PersonalizationAppUI() = default;

void PersonalizationAppUI::BindInterface(
    mojo::PendingReceiver<personalization_app::mojom::WallpaperProvider>
        receiver) {
  delegate_->BindInterface(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(PersonalizationAppUI)

}  // namespace ash
