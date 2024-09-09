// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/supervised_user/aw_supervised_user_blocking_page.h"

#include "android_webview/browser/aw_browser_context.h"
#include "base/i18n/rtl.h"
#include "components/grit/components_resources.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::unique_ptr<security_interstitials::MetricsHelper> GetMetricsHelper(
    const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "supervised_user_blocking_page";

  return std::make_unique<security_interstitials::MetricsHelper>(url, settings,
                                                                 nullptr);
}

}  // namespace

namespace android_webview {

AwSupervisedUserBlockingPage::AwSupervisedUserBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)) {}

AwSupervisedUserBlockingPage::~AwSupervisedUserBlockingPage() = default;

std::unique_ptr<security_interstitials::SecurityInterstitialPage>
AwSupervisedUserBlockingPage::CreateBlockingPage(
    content::WebContents* web_contents,
    const GURL& url) {
  AwBrowserContext* browser_context =
      AwBrowserContext::FromWebContents(web_contents);
  PrefService* pref_service = browser_context->GetPrefService();
  return make_unique<AwSupervisedUserBlockingPage>(
      web_contents, url,
      std::make_unique<
          security_interstitials::SecurityInterstitialControllerClient>(
          web_contents, GetMetricsHelper(url), pref_service,
          base::i18n::GetConfiguredLocale(), GURL(url::kAboutBlankURL),
          /*settings_page_helper=*/nullptr));
}

int AwSupervisedUserBlockingPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_SUPERVISED_USER_HTML;
}

void AwSupervisedUserBlockingPage::OnInterstitialClosing() {}

void AwSupervisedUserBlockingPage::CommandReceived(const std::string& command) {
  // TODO(crbug.com/359990680): Handle the
  // `security_interstitials::CMD_OPEN_HELP_CENTER` case.
}

void AwSupervisedUserBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  load_time_data.Set(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_URL_BLOCKED_MESSAGE));
}
}  // namespace android_webview
