// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/test_safe_browsing_blocking_page_quiet.h"

#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace security_interstitials {
TestSafeBrowsingBlockingPageQuiet::~TestSafeBrowsingBlockingPageQuiet() {}

TestSafeBrowsingBlockingPageQuiet::TestSafeBrowsingBlockingPageQuiet(
    safe_browsing::BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResourceList& unsafe_resources,
    const BaseSafeBrowsingErrorUI::SBErrorDisplayOptions& display_options,
    bool is_giant_webview)
    : BaseBlockingPage(ui_manager,
                       web_contents,
                       main_frame_url,
                       unsafe_resources,
                       CreateControllerClient(
                           web_contents,
                           unsafe_resources,
                           ui_manager,
                           nullptr,
                           /* settings_page_helper */ nullptr,
                           /* blocked_page_shown_timestamp */ std::nullopt),
                       display_options),
      sb_error_ui_(unsafe_resources[0].url,
                   GetInterstitialReason(unsafe_resources),
                   display_options,
                   ui_manager->app_locale(),
                   base::Time::NowFromSystemTime(),
                   controller(),
                   is_giant_webview) {}

// static
TestSafeBrowsingBlockingPageQuiet*
TestSafeBrowsingBlockingPageQuiet::CreateBlockingPage(
    safe_browsing::BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& main_frame_url,
    const UnsafeResource& unsafe_resource,
    bool is_giant_webview) {
  const UnsafeResourceList resources{unsafe_resource};

  BaseSafeBrowsingErrorUI::SBErrorDisplayOptions display_options =
      BaseBlockingPage::CreateDefaultDisplayOptions(resources);

  return new TestSafeBrowsingBlockingPageQuiet(
      ui_manager, web_contents, main_frame_url, resources, display_options,
      is_giant_webview);
}

std::string TestSafeBrowsingBlockingPageQuiet::GetHTML() {
  base::Value::Dict load_time_data;
  sb_error_ui_.PopulateStringsForHtml(load_time_data);
  webui::SetLoadTimeDataDefaults(controller()->GetApplicationLocale(),
                                 &load_time_data);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SECURITY_INTERSTITIAL_QUIET_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  return webui::GetLocalizedHtml(html, load_time_data);
}

}  // namespace security_interstitials
