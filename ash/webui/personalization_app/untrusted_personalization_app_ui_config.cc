// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/untrusted_personalization_app_ui_config.h"

#include "ash/constants/ash_features.h"
#include "ash/grit/ash_personalization_app_resources.h"
#include "ash/grit/ash_personalization_app_resources_map.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "base/strings/string_util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_generated_resources_map.h"
#include "ui/resources/grit/webui_resources.h"
#include "url/gurl.h"

namespace ash {

namespace {

void AddStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"myImagesLabel", IDS_PERSONALIZATION_APP_MY_IMAGES},
      {"zeroImages", IDS_PERSONALIZATION_APP_NO_IMAGES},
      {"oneImage", IDS_PERSONALIZATION_APP_ONE_IMAGE},
      {"multipleImages", IDS_PERSONALIZATION_APP_MULTIPLE_IMAGES},
      {"ariaLabelLoading", IDS_PERSONALIZATION_APP_ARIA_LABEL_LOADING}};
  source->AddLocalizedStrings(kLocalizedStrings);

  if (features::IsWallpaperGooglePhotosIntegrationEnabled()) {
    source->AddLocalizedString("googlePhotosLabel",
                               IDS_PERSONALIZATION_APP_GOOGLE_PHOTOS);
  }

  source->UseStringsJs();
}

void AddBooleans(content::WebUIDataSource* source) {
  source->AddBoolean("isGooglePhotosIntegrationEnabled",
                     features::IsWallpaperGooglePhotosIntegrationEnabled());
}

void AddCrosColors(content::WebUIDataSource* source) {
  source->AddResourcePath("chromeos/colors/cros_styles.css",
                          IDR_WEBUI_CROS_COLORS_CSS);

  source->AddString(
      "crosColorsDebugOverrides",
      base::FeatureList::IsEnabled(features::kSemanticColorsDebugOverride)
          ? cros_styles::kDebugOverrideCssString
          : std::string());
}

class UntrustedPersonalizationAppUI : public ui::UntrustedWebUIController {
 public:
  explicit UntrustedPersonalizationAppUI(content::WebUI* web_ui)
      : ui::UntrustedWebUIController(web_ui) {
    std::unique_ptr<content::WebUIDataSource> source =
        base::WrapUnique(content::WebUIDataSource::Create(
            kChromeUIUntrustedPersonalizationAppURL));

    AddStrings(source.get());
    AddBooleans(source.get());

    const auto resources = base::make_span(kAshPersonalizationAppResources,
                                           kAshPersonalizationAppResourcesSize);

    for (const auto& resource : resources) {
      if (base::StartsWith(resource.path, "untrusted") ||
          base::StartsWith(resource.path, "common"))
        source->AddResourcePath(resource.path, resource.id);
    }
    // Add WebUI resources like polymer and iron-list so that it is accessible
    // inside untrusted iframe.
    source->AddResourcePaths(base::make_span(kWebuiGeneratedResources,
                                             kWebuiGeneratedResourcesSize));

    AddCrosColors(source.get());

    source->AddFrameAncestor(GURL(kChromeUIPersonalizationAppURL));

    // Allow images only from this url.
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ImgSrc,
        "img-src 'self' data: https://*.googleusercontent.com;");

    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src 'self' chrome-untrusted://resources;");

    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::StyleSrc,
        "style-src 'self' 'unsafe-inline';");

#if !DCHECK_IS_ON()
    // When DCHECKs are off and a user goes to an invalid url serve a default
    // page to avoid crashing. We crash when DCHECKs are on to make it clearer
    // that a resource path was not property specified.
    source->SetDefaultResource(
        IDR_ASH_PERSONALIZATION_APP_UNTRUSTED_COLLECTIONS_HTML);
#endif  // !DCHECK_IS_ON()

    // TODO(crbug/1169829) set up trusted types properly to allow Polymer to
    // write html.
    source->DisableTrustedTypesCSP();

    auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
    content::WebUIDataSource::Add(browser_context, source.release());
  }

  UntrustedPersonalizationAppUI(const UntrustedPersonalizationAppUI&) = delete;
  UntrustedPersonalizationAppUI& operator=(
      const UntrustedPersonalizationAppUI&) = delete;
  ~UntrustedPersonalizationAppUI() override = default;
};

}  // namespace

UntrustedPersonalizationAppUIConfig::UntrustedPersonalizationAppUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  kChromeUIPersonalizationAppHost) {}

UntrustedPersonalizationAppUIConfig::~UntrustedPersonalizationAppUIConfig() =
    default;

bool UntrustedPersonalizationAppUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return features::IsWallpaperWebUIEnabled() &&
         !browser_context->IsOffTheRecord();
}

std::unique_ptr<content::WebUIController>
UntrustedPersonalizationAppUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  return std::make_unique<UntrustedPersonalizationAppUI>(web_ui);
}

}  // namespace ash
