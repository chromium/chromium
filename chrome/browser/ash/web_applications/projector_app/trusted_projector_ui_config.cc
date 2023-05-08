// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/projector_app/trusted_projector_ui_config.h"

#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "ash/webui/projector_app/trusted_projector_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/projector/projector_utils.h"

namespace ash {

namespace {

std::unique_ptr<content::WebUIController> BindCreateWebUIControllerFunc(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<ash::TrustedProjectorUI>(
      web_ui, url, Profile::FromWebUI(web_ui)->GetPrefs());
}

}  // namespace

TrustedProjectorUIConfig::TrustedProjectorUIConfig()
    : SystemWebAppUIConfig(
          ash::kChromeUIProjectorAppHost,
          SystemWebAppType::PROJECTOR,
          base::BindRepeating(&BindCreateWebUIControllerFunc)) {}

bool TrustedProjectorUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  if (!SystemWebAppUIConfig::IsWebUIEnabled(browser_context)) {
    return false;
  }
  return IsProjectorAppEnabled(Profile::FromBrowserContext(browser_context));
}

}  // namespace ash
