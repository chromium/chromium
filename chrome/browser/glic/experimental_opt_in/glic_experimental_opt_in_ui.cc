// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/glic_experimental_opt_in_resources.h"
#include "chrome/grit/glic_experimental_opt_in_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_util.h"

namespace glic {

GlicExperimentalOptInUIConfig::GlicExperimentalOptInUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIGlicExperimentalOptInHost) {}

GlicExperimentalOptInUIConfig::~GlicExperimentalOptInUIConfig() = default;

bool GlicExperimentalOptInUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kGlicExperimentalTriggering) &&
         GlicEnabling::IsProfileEligible(
             Profile::FromBrowserContext(browser_context));
}

GlicExperimentalOptInUI::GlicExperimentalOptInUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIGlicExperimentalOptInHost);

  webui::SetupWebUIDataSource(
      source, kGlicExperimentalOptInResources,
      IDR_GLIC_EXPERIMENTAL_OPT_IN_EXPERIMENTAL_OPT_IN_HTML);
}

GlicExperimentalOptInUI::~GlicExperimentalOptInUI() = default;

}  // namespace glic
