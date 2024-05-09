// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/focus_mode/focus_mode_ui.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/url_constants.h"
#include "ash/webui/common/trusted_types_util.h"
#include "ash/webui/grit/ash_focus_mode_resources.h"
#include "ash/webui/grit/ash_focus_mode_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace ash {

FocusModeUI::FocusModeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://focus-mode-media source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIFocusModeMediaHost);

  // Setup chrome://focus-mode-media main page.
  source->AddResourcePath("", IDR_ASH_FOCUS_MODE_FOCUS_MODE_HTML);
  // Add chrome://focus-mode-media content.
  source->AddResourcePaths(
      base::make_span(kAshFocusModeResources, kAshFocusModeResourcesSize));

  ash::EnableTrustedTypesCSP(source);
}

FocusModeUI::~FocusModeUI() = default;

FocusModeUIConfig::FocusModeUIConfig()
    : WebUIConfig(content::kChromeUIScheme,
                  chrome::kChromeUIFocusModeMediaHost) {}

FocusModeUIConfig::~FocusModeUIConfig() = default;

std::unique_ptr<content::WebUIController>
FocusModeUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                         const GURL& url) {
  return std::make_unique<FocusModeUI>(web_ui);
}

bool FocusModeUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::features::IsFocusModeEnabled();
}

}  // namespace ash
