// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/crosh_ui.h"

#include "chrome/browser/ash/system_web_apps/apps/terminal_source.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"

CroshUIConfig::CroshUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         chrome::kChromeUIUntrustedCroshHost) {}

CroshUIConfig::~CroshUIConfig() = default;

bool CroshUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  // TODO(b/210659944): Make logic depend on SystemFeaturesDisableList policy.
  return true;
}

CroshUI::CroshUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  auto* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(profile, TerminalSource::ForCrosh(profile));
}

CroshUI::~CroshUI() = default;
