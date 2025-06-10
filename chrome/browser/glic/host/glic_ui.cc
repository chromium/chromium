// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_ui.h"

#include <string>

#include "base/command_line.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/host/glic_page_handler.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/glic_resources.h"
#include "chrome/grit/glic_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/webui_allowlist.h"
#include "ui/webui/webui_util.h"

namespace glic {

// static
bool GlicUI::simulate_no_connection_ = false;

GlicUIConfig::GlicUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme, chrome::kChromeUIGlicHost) {}

bool GlicUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return GlicEnabling::IsProfileEligible(
      Profile::FromBrowserContext(browser_context));
}

GlicUI::GlicUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"closeButtonLabel", IDS_GLIC_NOTICE_CLOSE_BUTTON_LABEL},
      {"errorNotice", IDS_GLIC_ERROR_NOTICE},
      {"errorNoticeActionButton", IDS_GLIC_ERROR_NOTICE_ACTION_BUTTON},
      {"errorNoticeHeader", IDS_GLIC_ERROR_NOTICE_HEADER},
      {"offlineNoticeAction", IDS_GLIC_OFFLINE_NOTICE_ACTION},
      {"offlineNoticeActionButton", IDS_GLIC_OFFLINE_NOTICE_ACTION_BUTTON},
      {"offlineNoticeHeader", IDS_GLIC_OFFLINE_NOTICE_HEADER},
      {"signInNotice", IDS_GLIC_SIGN_IN_NOTICE},
      {"signInNoticeActionButton", IDS_GLIC_SIGN_IN_NOTICE_ACTION_BUTTON},
      {"signInNoticeHeader", IDS_GLIC_SIGN_IN_NOTICE_HEADER},
      {"unresponsiveMessage", IDS_GLIC_UNRESPONSIVE_MESSAGE},
  };

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Set up the chrome://glic source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIGlicHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kGlicResources, IDR_GLIC_GLIC_HTML);

  // Add localized strings.
  source->AddLocalizedStrings(kStrings);

  // Register auto-granted permissions.
  auto* allowlist = WebUIAllowlist::GetOrCreate(browser_context);
  allowlist->RegisterAutoGrantedPermission(source->GetOrigin(),
                                           ContentSettingsType::GEOLOCATION);

  auto* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_glic_dev = command_line->HasSwitch(::switches::kGlicDev);

  source->AddString("chromeVersion", version_info::GetVersionNumber());
  source->AddString("chromeChannel",
                    version_info::GetChannelString(chrome::GetChannel()));
  source->AddBoolean("loggingEnabled",
                     command_line->HasSwitch(::switches::kGlicHostLogging));
  // Set up guest URL via cli flag or default to finch param value.
  source->AddString("glicGuestURL", GetGuestURL().spec());

  // Set up loading notice timeout values.
  source->AddInteger("preLoadingTimeMs", features::kGlicPreLoadingTimeMs.Get());
  source->AddInteger("minLoadingTimeMs", features::kGlicMinLoadingTimeMs.Get());
  int max_loading_time_ms = features::kGlicMaxLoadingTimeMs.Get();
  if (is_glic_dev) {
    // Bump up timeout value, as dev server may be slow.
    max_loading_time_ms *= 100;
  }
  source->AddInteger("maxLoadingTimeMs", max_loading_time_ms);
  source->AddBoolean("simulateNoConnection", simulate_no_connection_);

  source->AddResourcePath("glic_logo.svg", IDR_GLIC_LOGO);

  // Set up guest api source.
  // This comes from 'glic_api_injection' in
  // chrome/browser/resources/glic/BUILD.gn.
  source->AddString(
      "glicGuestAPISource",
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_GLIC_GLIC_API_IMPL_GLIC_API_INJECTED_CLIENT_ROLLUP_JS));

  std::string allowed_origins =
      command_line->GetSwitchValueASCII(::switches::kGlicAllowedOrigins);
  if (allowed_origins.empty()) {
    allowed_origins = features::kGlicAllowedOriginsOverride.Get();
  }
  if (allowed_origins.empty()) {
    // TODO(crbug.com/396147389): Replace with the correct default.
    allowed_origins = "https://*.google.com/";
  }
  source->AddString("glicAllowedOrigins", allowed_origins);

  source->AddBoolean("devMode", is_glic_dev);

  source->AddBoolean("enableDebug",
                     base::FeatureList::IsEnabled(features::kGlicDebugWebview));

  // Set up for periodic web client responsiveness check and its interval,
  // timeout, and max unresponsive ui time.
  source->AddBoolean(
      "isClientResponsivenessCheckEnabled",
      base::FeatureList::IsEnabled(features::kGlicClientResponsivenessCheck));
  source->AddInteger("clientResponsivenessCheckIntervalMs",
                     features::kGlicClientResponsivenessCheckIntervalMs.Get());
  source->AddInteger("clientResponsivenessCheckTimeoutMs",
                     features::kGlicClientResponsivenessCheckTimeoutMs.Get());
  source->AddInteger("clientUnresponsiveUiMaxTimeMs",
                     features::kGlicClientUnresponsiveUiMaxTimeMs.Get());
  source->AddBoolean("enableWebClientUnresponsiveMetrics",
                     base::FeatureList::IsEnabled(
                         features::kGlicWebClientUnresponsiveMetrics));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicUI)

GlicUI::~GlicUI() = default;

void GlicUI::BindInterface(
    mojo::PendingReceiver<glic::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void GlicUI::CreatePageHandler(
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<glic::mojom::Page> page) {
  page_handler_ = std::make_unique<GlicPageHandler>(
      web_ui()->GetWebContents(), std::move(receiver), std::move(page));
}

}  // namespace glic
