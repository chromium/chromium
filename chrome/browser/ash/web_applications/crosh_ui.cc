// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/crosh_ui.h"

#include "chrome/browser/ash/web_applications/terminal_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

CroshUIConfig::CroshUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIUntrustedCroshHost) {}

CroshUIConfig::~CroshUIConfig() = default;

bool CroshUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  // TODO(b/210659944): Make logic depend on SystemFeaturesDisableList policy.
  return true;
}

std::unique_ptr<content::WebUIController> CroshUIConfig::CreateWebUIController(
    content::WebUI* web_ui,
    const GURL& url) {
  return std::make_unique<CroshUI>(web_ui);
}

CroshUI::CroshUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, TerminalSource::ForCrosh(profile));
}

CroshUI::~CroshUI() = default;
