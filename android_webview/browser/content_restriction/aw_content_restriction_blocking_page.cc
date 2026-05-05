// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/content_restriction/aw_content_restriction_blocking_page.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/content_restriction/aw_content_restriction_manager_client.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/rtl.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "components/grit/components_resources.h"
#include "components/prefs/pref_service.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/content/settings_page_helper.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

namespace android_webview {
namespace {
std::unique_ptr<security_interstitials::MetricsHelper> GetMetricsHelper(
    const GURL& url) {
  security_interstitials::MetricsHelper::ReportDetails settings;
  settings.metric_prefix = "content_restriction_blocking_page";

  return std::make_unique<security_interstitials::MetricsHelper>(url, settings,
                                                                 nullptr);
}
}  // namespace

// static
std::unique_ptr<security_interstitials::SecurityInterstitialPage>
AwContentRestrictionBlockingPage::CreateBlockingPage(
    content::WebContents* web_contents,
    const GURL& url,
    AwContentRestrictionManagerClient* content_restriction_manager_client) {
  const AwBrowserContext* const browser_context =
      AwBrowserContext::FromWebContents(web_contents);
  PrefService* const pref_service = browser_context->GetPrefService();
  return base::WrapUnique(new AwContentRestrictionBlockingPage(
      web_contents, url, content_restriction_manager_client,
      std::make_unique<
          security_interstitials::SecurityInterstitialControllerClient>(
          web_contents, GetMetricsHelper(url), pref_service,
          base::i18n::GetConfiguredLocale(), GURL(url::kAboutBlankURL),
          /*settings_page_helper=*/nullptr)));
}

AwContentRestrictionBlockingPage::AwContentRestrictionBlockingPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    AwContentRestrictionManagerClient* client,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      content_restriction_manager_client_(client) {}

AwContentRestrictionBlockingPage::~AwContentRestrictionBlockingPage() = default;

int AwContentRestrictionBlockingPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_CONTENT_RESTRICTION_HTML;
}

void AwContentRestrictionBlockingPage::OnInterstitialClosing() {}

bool AwContentRestrictionBlockingPage::ShouldDisplayURL() const {
  return false;
}

void AwContentRestrictionBlockingPage::CommandReceived(
    const std::string& command) {
  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  switch (cmd) {
    case security_interstitials::CMD_DONT_PROCEED:
      DCHECK(web_contents());
      if (web_contents()->GetController().CanGoBack()) {
        web_contents()->GetController().GoBack();
      }
      break;
    case security_interstitials::CMD_SHOW_MORE_SECTION: {
      DCHECK(content_restriction_manager_client_);
      content_restriction_manager_client_->SendShowRestrictedContentIntent(
          request_url());
      break;
    }
    default:
      NOTREACHED();
  }
}

void AwContentRestrictionBlockingPage::PopulateInterstitialStrings(
    base::DictValue& load_time_data) {
  load_time_data.Set("textParagraph",
                     l10n_util::GetStringUTF16(
                         IDS_WEBVIEW_CONTENT_RESTRICTION_BLOCKED_MESSAGE));
  load_time_data.Set(
      "learnMoreText",
      l10n_util::GetStringUTF16(IDS_WEBVIEW_CONTENT_RESTRICTION_LEARN_MORE));
  load_time_data.Set("backText", l10n_util::GetStringUTF16(
                                     IDS_WEBVIEW_CONTENT_RESTRICTION_GO_BACK));
}

}  // namespace android_webview
