// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_untrusted_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_colors.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/glic_untrusted_resources.h"
#include "chrome/grit/glic_untrusted_resources_map.h"
#include "chrome/grit/lens_untrusted_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace glic {

bool SelectionOverlayUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return GlicEnabling::IsProfileEligible(
      Profile::FromBrowserContext(browser_context));
}

SelectionOverlayUntrustedUI::SelectionOverlayUntrustedUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui) {
  // Set up the chrome-untrusted://glic/selection-overlay source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIGlicUntrustedURL);
  CHECK(html_source);
  webui::SetupWebUIDataSource(html_source, kGlicUntrustedResources,
                              IDR_GLIC_UNTRUSTED_SELECTION_OVERLAY_HTML);
  html_source->AddLocalizedString(
      "searchScreenshot",
      IDS_LENS_OVERLAY_SEARCH_SCREENSHOT_ACCESSIBILITY_LABEL);
  html_source->AddBoolean("enableShimmer", true);
  html_source->AddBoolean("enableBorderGlow", true);
  html_source->AddBoolean("enableKeyboardSelection", false);
  html_source->AddInteger("tapRegionHeight", 300);
  html_source->AddInteger("tapRegionWidth", 300);
  html_source->AddBoolean("enableGradientRegionStroke", false);
  html_source->AddBoolean("enableWhiteRegionStroke", true);
  html_source->AddBoolean("enableRegionSelectedGlow", true);
  html_source->AddInteger("sliderChangedTimeout", 1000);
  html_source->AddBoolean("cornerSlidersEnabled", true);
  html_source->AddBoolean("enableShimmerSparkles", true);
  html_source->AddInteger("colorFallbackPrimary", lens::kColorFallbackPrimary);
  html_source->AddInteger("colorFallbackShaderLayer1",
                          lens::kColorFallbackShaderLayer1);
  html_source->AddInteger("colorFallbackShaderLayer2",
                          lens::kColorFallbackShaderLayer2);
  html_source->AddInteger("colorFallbackShaderLayer3",
                          lens::kColorFallbackShaderLayer3);
  html_source->AddInteger("colorFallbackShaderLayer4",
                          lens::kColorFallbackShaderLayer4);
  html_source->AddInteger("colorFallbackShaderLayer5",
                          lens::kColorFallbackShaderLayer5);
  html_source->AddInteger("colorFallbackScrim", lens::kColorFallbackScrim);
  html_source->AddInteger("colorFallbackSurfaceContainerHighestLight",
                          lens::kColorFallbackSurfaceContainerHighestLight);
  html_source->AddInteger("colorFallbackSurfaceContainerHighestDark",
                          lens::kColorFallbackSurfaceContainerHighestDark);
  html_source->AddInteger("colorFallbackSelectionElement",
                          lens::kColorFallbackSelectionElement);
  html_source->AddLocalizedString(
      "topLeftSliderAriaLabel",
      IDS_LENS_OVERLAY_TOP_LEFT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "topRightSliderAriaLabel",
      IDS_LENS_OVERLAY_TOP_RIGHT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "bottomRightSliderAriaLabel",
      IDS_LENS_OVERLAY_BOTTOM_RIGHT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddLocalizedString(
      "bottomLeftSliderAriaLabel",
      IDS_LENS_OVERLAY_BOTTOM_LEFT_CORNER_SLIDER_ACCESSIBILITY_LABEL);
  html_source->AddBoolean("enableMultiRegionSelection", true);
  html_source->AddLocalizedString("close", IDS_CLOSE);
  html_source->AddResourcePath("glic_region_selection_cursor_icon.svg",
                               IDR_GLIC_REGION_SELECTION_CURSOR_ICON);

  // TODO(b/489801993): Refactor shared resources into a common directory to
  // avoid manual path concatenation for Lens and the Glic selection overlay.
  for (const auto& resource : kLensUntrustedResources) {
    html_source->AddResourcePath(base::StrCat({"lens/", resource.path}),
                                 resource.id);

    std::string_view path(resource.path);
    if (path.contains(".svg") ||
        path.contains("post_selection_paint_worklet.js")) {
      html_source->AddResourcePath(
          base::StrCat({"selection-overlay/", resource.path}), resource.id);
    }
  }
}

SelectionOverlayUntrustedUI::~SelectionOverlayUntrustedUI() = default;

void SelectionOverlayUntrustedUI::BindInterface(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandlerFactory>
        receiver) {
  selection_page_factory_receiver_.reset();
  selection_page_factory_receiver_.Bind(std::move(receiver));
}

SelectionOverlayController&
SelectionOverlayUntrustedUI::GetSelectionOverlayController() {
  SelectionOverlayController* controller =
      SelectionOverlayController::FromOverlayWebContents(
          web_ui()->GetWebContents());
  return *controller;
}

void SelectionOverlayUntrustedUI::CreatePageHandler(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
    mojo::PendingRemote<selection::SelectionOverlayPage> page) {
  SelectionOverlayController& controller = GetSelectionOverlayController();
  controller.BindOverlay(std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(SelectionOverlayUntrustedUI)

}  // namespace glic
