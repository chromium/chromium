// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_ui.h"

#include <string>

#include "base/command_line.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/fre/glic_fre_page_handler.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/glic_fre_resources.h"
#include "chrome/grit/glic_fre_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/webui_allowlist.h"
#include "ui/webui/webui_util.h"

namespace glic {

GlicFreUIConfig::GlicFreUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIGlicFreHost) {}

bool GlicFreUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return GlicEnabling::IsProfileEligible(
      Profile::FromBrowserContext(browser_context));
}

GlicFreUI::GlicFreUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"closeButtonLabel", IDS_GLIC_NOTICE_CLOSE_BUTTON_LABEL},
      {"errorNotice", IDS_GLIC_ERROR_NOTICE},
      {"errorNoticeActionButton", IDS_GLIC_ERROR_NOTICE_ACTION_BUTTON},
      {"errorNoticeHeader", IDS_GLIC_ERROR_NOTICE_HEADER},
      {"offlineNoticeAction", IDS_GLIC_OFFLINE_NOTICE_ACTION},
      {"offlineNoticeActionButton", IDS_GLIC_OFFLINE_NOTICE_ACTION_BUTTON},
      {"offlineNoticeHeader", IDS_GLIC_OFFLINE_NOTICE_HEADER},
  };

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Set up the chrome://glic-fre source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, chrome::kChromeUIGlicFreHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kGlicFreResources, IDR_GLIC_FRE_FRE_HTML);

  // Add localized strings.
  source->AddLocalizedStrings(kStrings);

  // Set up FRE URL via cli flag, or default to the finch param value.
  source->AddString(
      "glicFreURL",
      GetFreURL(Profile::FromBrowserContext(browser_context)).spec());
  source->AddInteger("freInitialWidth", features::kGlicFreInitialWidth.Get());
  source->AddInteger("freInitialHeight", features::kGlicFreInitialHeight.Get());

  auto* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_glic_dev = command_line->HasSwitch(::switches::kGlicDev);

  // Set up loading notice timeout values.
  source->AddInteger("preLoadingTimeMs", features::kGlicPreLoadingTimeMs.Get());
  source->AddInteger("minLoadingTimeMs", features::kGlicMinLoadingTimeMs.Get());
  int max_loading_time_ms = features::kGlicMaxLoadingTimeMs.Get();
  int reload_max_loading_time_ms = features::kGlicReloadMaxLoadingTimeMs.Get();
  if (is_glic_dev) {
    // Bump up timeout value, as dev server may be slow.
    max_loading_time_ms *= 100;
  }
  source->AddInteger("maxLoadingTimeMs", max_loading_time_ms);
  source->AddInteger("reloadMaxLoadingTimeMs", reload_max_loading_time_ms);
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicFreUI)

GlicFreUI::~GlicFreUI() = default;

void GlicFreUI::BindInterface(
    mojo::PendingReceiver<glic::mojom::FrePageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void GlicFreUI::CreatePageHandler(
    mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver) {
  fre_page_handler_ = std::make_unique<GlicFrePageHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

}  // namespace glic
