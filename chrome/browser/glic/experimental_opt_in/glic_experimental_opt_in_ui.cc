// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/glic_experimental_opt_in_resources.h"
#include "chrome/grit/glic_experimental_opt_in_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "ui/webui/webui_util.h"

namespace glic {

namespace {

GURL GetExperimentalTriggeringOptInURL(Profile* profile,
                                       RequiredExperimentalOptIn state) {
  GURL url = GURL(features::kGlicExperimentalTriggeringOptInURL.Get());
  if (url.is_empty()) {
    LOG(ERROR) << "No glic experimental triggering opt in url";
    return GURL();
  }

  std::string state_str;
  switch (state) {
    case RequiredExperimentalOptIn::kGlic:
      state_str = "glic";
      break;
    case RequiredExperimentalOptIn::kActuation:
      state_str = "actuation";
      break;
    case RequiredExperimentalOptIn::kExperimental:
      state_str = "experimental";
      break;
    case RequiredExperimentalOptIn::kNotNeeded:
      NOTREACHED();
  }

  url = net::AppendOrReplaceQueryParameter(
      url, "experimental_triggering_opt_in", state_str);

  return DecorateGlicFreUrl(profile, url);
}

}  // namespace

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

  auto state = GlicKeyedServiceFactory::GetGlicKeyedService(profile)
                   ->enabling()
                   .GetRequiredExperimentalOptIn();

  if (state == RequiredExperimentalOptIn::kNotNeeded) {
    // It's theoretically possible that between the decision by the controller
    // to show the dialog, and when the dialog actually loads and this code
    // executes, opt-in is no longer required (e.g. changing toggle in a
    // different tab). That case should be very rare, and we can fallback to the
    // experimental opt-in.
    // TODO(b/511184397): Add metrics for how often this happens.
    state = RequiredExperimentalOptIn::kExperimental;
  }

  GURL url = GetExperimentalTriggeringOptInURL(profile, state);
  source->AddString("glicExperimentalTriggeringOptInURL", url.spec());
}

GlicExperimentalOptInUI::~GlicExperimentalOptInUI() = default;

}  // namespace glic
