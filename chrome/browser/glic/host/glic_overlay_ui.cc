// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_overlay_ui.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/glic_resources.h"
#include "chrome/grit/glic_resources_map.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/webui_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

namespace glic {

GlicOverlayUI::GlicOverlayUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              /*enable_chrome_send=*/false,
                              /*enable_chrome_histograms=*/true) {
  static constexpr webui::LocalizedString kStrings[] = {
      {"closeButtonLabel", IDS_GLIC_NOTICE_CLOSE_BUTTON_LABEL},
      {"errorNotice", IDS_GLIC_ERROR_NOTICE},
      {"errorNoticeActionButton", IDS_GLIC_ERROR_NOTICE_ACTION_BUTTON},
      {"errorNoticeHeader", IDS_GLIC_ERROR_NOTICE_HEADER},
      {"showErrorButton", IDS_GLIC_SHOW_ERROR_BUTTON},
      {"ineligibleProfileNotice", IDS_GLIC_INELIGIBLE_PROFILE_NOTICE},
      {"ineligibleProfileNoticeActionButton",
       IDS_GLIC_INELIGIBLE_PROFILE_NOTICE_ACTION_BUTTON},
      {"ineligibleProfileNoticeHeader",
       IDS_GLIC_INELIGIBLE_PROFILE_NOTICE_HEADER},
      {"ineligibleAccountNotice", IDS_GLIC_INELIGIBLE_ACCOUNT_NOTICE},
      {"ineligibleAccountNoticeHeader",
       IDS_GLIC_INELIGIBLE_ACCOUNT_NOTICE_HEADER},
      {"disabledByAdminNotice", IDS_GLIC_DISABLED_BY_ADMIN_NOTICE},
      {"disabledByAdminNoticeCloseButton",
       IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_CLOSE_BUTTON},
      {"disabledByAdminNoticeHeader", IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_HEADER},
      {"offlineNoticeAction", IDS_GLIC_OFFLINE_NOTICE_ACTION},
      {"offlineNoticeActionButton", IDS_GLIC_OFFLINE_NOTICE_ACTION_BUTTON},
      {"offlineNoticeHeader", IDS_GLIC_OFFLINE_NOTICE_HEADER},
      {"zoomLabel", IDS_TOOLTIP_ZOOM},
      {"signInNotice", IDS_GLIC_SIGN_IN_NOTICE},
      {"signInNoticeActionButton", IDS_GLIC_SIGN_IN_NOTICE_ACTION_BUTTON},
      {"signInNoticeHeader", IDS_GLIC_SIGN_IN_NOTICE_HEADER},
      {"unresponsiveMessage", IDS_GLIC_UNRESPONSIVE_MESSAGE},
      {"locationMismatchNoticeHeader",
       IDS_GLIC_LOCATION_MISMATCH_NOTICE_HEADER},
      {"locationMismatchNotice", IDS_GLIC_LOCATION_MISMATCH_NOTICE},
      {"getHelp", IDS_GLIC_GET_HELP},
  };

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIGlicHost);

#if !BUILDFLAG(IS_ANDROID)
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
#endif

  webui::SetupWebUIDataSource(source, kGlicResources,
                              IDR_GLIC_GLIC_OVERLAY_HTML);
  source->AddResourcePath("overlay", IDR_GLIC_GLIC_OVERLAY_HTML);
  source->AddResourcePath("overlay/", IDR_GLIC_GLIC_OVERLAY_HTML);
  source->AddLocalizedStrings(kStrings);

  source->AddBoolean("isNoWebview", true);
  source->AddBoolean("isAndroidMobile", false);

  source->AddString("disabledByAdminNoticeWithLink",
                    l10n_util::GetStringFUTF16(
                        IDS_GLIC_DISABLED_BY_ADMIN_NOTICE_WITH_LINK,
                        base::UTF8ToUTF16(features::kGlicCaaLinkUrl.Get()),
                        base::UTF8ToUTF16(features::kGlicCaaLinkText.Get())));
}

WEB_UI_CONTROLLER_TYPE_IMPL(GlicOverlayUI)

GlicOverlayUI::~GlicOverlayUI() = default;

void GlicOverlayUI::SetPageHandler(
    mojom::GlicOverlayPageHandler* page_handler) {
  // Set or update the C++ page handler. If the WebUI renderer already invoked
  // CreatePageHandler before the navigation committed, bind the pending
  // receiver now.
  page_handler_ = page_handler;
  if (page_handler_ && pending_receiver_.is_valid()) {
    page_handler_receiver_.emplace(page_handler_, std::move(pending_receiver_));
  }
}

void GlicOverlayUI::SetOverlayState(mojom::OverlayStatePtr state) {
  current_state_ = std::move(state);
  if (page_remote_.is_bound() && current_state_) {
    page_remote_->SetOverlayState(current_state_.Clone());
  }
}

void GlicOverlayUI::BindInterface(
    mojo::PendingReceiver<mojom::GlicOverlayPageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void GlicOverlayUI::CreatePageHandler(
    mojo::PendingRemote<mojom::GlicOverlayPage> page,
    mojo::PendingReceiver<mojom::GlicOverlayPageHandler> receiver) {
  page_remote_.reset();
  page_remote_.Bind(std::move(page));
  if (page_handler_) {
    page_handler_receiver_.emplace(page_handler_, std::move(receiver));
  } else {
    pending_receiver_ = std::move(receiver);
  }
  if (current_state_) {
    page_remote_->SetOverlayState(current_state_.Clone());
  }
}

}  // namespace glic
