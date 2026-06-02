// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_ui.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_metrics.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_page_handler.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_util.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
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
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool has_url_override =
      command_line->HasSwitch(::switches::kGlicExperimentalFreURL);
  GURL url = GURL(has_url_override
                      ? command_line->GetSwitchValueASCII(
                            ::switches::kGlicExperimentalFreURL)
                      : features::kGlicExperimentalTriggeringOptInURL.Get());
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

  return DecorateGlicOptInUrl(profile, url);
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
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIGlicExperimentalOptInHost);

  webui::SetupWebUIDataSource(
      source, kGlicExperimentalOptInResources,
      IDR_GLIC_EXPERIMENTAL_OPT_IN_EXPERIMENTAL_OPT_IN_HTML);

  GlicKeyedService* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  required_state_ = service->enabling().GetRequiredExperimentalOptIn();
  RecordExperimentalOptInShown(required_state_);
  if (required_state_ == RequiredExperimentalOptIn::kGlic) {
    service->metrics()->OnOptInShown(OptInFlow::kExperimentalTriggering);
  }

  if (required_state_ == RequiredExperimentalOptIn::kNotNeeded) {
    // It's theoretically possible that between the decision by the controller
    // to show the dialog, and when the dialog actually loads and this code
    // executes, opt-in is no longer required (e.g. changing toggle in a
    // different tab). That case should be very rare, and we can fallback to the
    // experimental opt-in.
    required_state_ = RequiredExperimentalOptIn::kExperimental;
  }

  GURL url = GetExperimentalTriggeringOptInURL(profile, required_state_);
  source->AddString("glicExperimentalTriggeringOptInURL", url.spec());

  static constexpr webui::LocalizedString kStrings[] = {
      {"offlineNoticeHeader", IDS_GLIC_OFFLINE_NOTICE_HEADER},
      {"experimentalOptInOfflineNoticeMessage",
       IDS_GLIC_EXPERIMENTAL_OPT_IN_OFFLINE_NOTICE_MESSAGE},
      {"closeButtonLabel", IDS_GLIC_NOTICE_CLOSE_BUTTON_LABEL},
      {"errorNoticeHeader", IDS_GLIC_ERROR_NOTICE_HEADER},
      {"experimentalOptInErrorNoticeMessage", IDS_GLIC_ERROR_NOTICE},
      {"tryAgainButtonLabel", IDS_GLIC_ERROR_NOTICE_ACTION_BUTTON},
  };
  source->AddLocalizedStrings(kStrings);
}

GlicExperimentalOptInUI::~GlicExperimentalOptInUI() = default;

void GlicExperimentalOptInUI::BindInterface(
    mojo::PendingReceiver<mojom::ExperimentalOptInPageHandler> receiver) {
  page_handler_ = std::make_unique<GlicExperimentalOptInPageHandler>(
      Profile::FromWebUI(web_ui()), required_state_, std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicExperimentalOptInUI)

}  // namespace glic
