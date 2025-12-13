// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_ui.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/glic/fre/fre_util.h"
#include "chrome/browser/glic/fre/glic_fre_page_handler.h"
#include "chrome/browser/glic/glic_net_log.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/glic_resources.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/shared/webui_shared.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/glic_fre_resources.h"
#include "chrome/grit/glic_fre_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
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
      {"disabledByAdminNoticeCloseButton",
       IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_CLOSE_BUTTON},
      {"disabledByAdminNoticeHeader", IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_HEADER},
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
  ConfigureSharedWebUISource(*source);

  source->AddResourcePath("glic_logo.svg", GetResourceID(IDR_GLIC_LOGO));

  // Add required resources.
  webui::SetupWebUIDataSource(source, kGlicFreResources, IDR_GLIC_FRE_FRE_HTML);

  // Add localized strings.
  source->AddLocalizedStrings(kStrings);

  // Add parameterized admin notice string.
  source->AddString("disabledByAdminNoticeWithLink",
                    l10n_util::GetStringFUTF16(
                        IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_WITH_LINK,
                        base::UTF8ToUTF16(features::kGlicCaaLinkUrl.Get()),
                        base::UTF8ToUTF16(features::kGlicCaaLinkText.Get())));
  // Set up FRE URL via cli flag, or default to the finch param value.
  const GURL guest_url =
      GetFreURL(Profile::FromBrowserContext(browser_context));
  source->AddString("glicFreURL", guest_url.spec());
  net_log::LogDummyNetworkRequestForTrafficAnnotation(
      guest_url, net_log::GlicPage::kGlicFre);
  source->AddInteger("freInitialWidth", features::kGlicFreInitialWidth.Get());
  source->AddInteger("freInitialHeight", features::kGlicFreInitialHeight.Get());

  int reload_max_loading_time_ms = features::kGlicReloadMaxLoadingTimeMs.Get();
  source->AddInteger("reloadMaxLoadingTimeMs", reload_max_loading_time_ms);
  source->AddBoolean("isUnifiedFre",
                     GlicEnabling::IsUnifiedFreEnabled(
                         Profile::FromBrowserContext(browser_context)));
  source->AddBoolean("caaGuestError", base::FeatureList::IsEnabled(
                                          features::kGlicCaaGuestError));
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
      /*is_unified_fre=*/false, web_ui()->GetWebContents(),
      std::move(receiver));
}

}  // namespace glic
