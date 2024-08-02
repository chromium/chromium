// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_verification_page.h"

#include <utility>

#include "chrome/browser/signin/signin_promo.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using security_interstitials::MetricsHelper;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    SupervisedUserVerificationPage::kTypeForTesting =
        &SupervisedUserVerificationPage::kTypeForTesting;

SupervisedUserVerificationPage::SupervisedUserVerificationPage(
    content::WebContents* web_contents,
    const std::string& email_to_reauth,
    const GURL& request_url,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      email_to_reauth_(email_to_reauth),
      request_url_(request_url) {}

SupervisedUserVerificationPage::~SupervisedUserVerificationPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
SupervisedUserVerificationPage::GetTypeForTesting() {
  return SupervisedUserVerificationPage::kTypeForTesting;
}

void SupervisedUserVerificationPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  PopulateStringsForSharedHTML(load_time_data);
  load_time_data.Set("tabTitle", l10n_util::GetStringUTF16(
                                     IDS_SUPERVISED_USER_VERIFY_IT_IS_YOU));
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));
  load_time_data.Set("heading", l10n_util::GetStringUTF16(
                                    IDS_SUPERVISED_USER_VERIFY_IT_IS_YOU));
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringUTF16(
                         IDS_SUPERVISED_USER_VERIFY_PAGE_PRIMARY_PARAGRAPH));
  load_time_data.Set(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_VERIFY_IT_IS_YOU));
}

void SupervisedUserVerificationPage::OnInterstitialClosing() {}

int SupervisedUserVerificationPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

void SupervisedUserVerificationPage::PopulateStringsForSharedHTML(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);

  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");

  load_time_data.Set("type", "SUPERVISED_USER_VERIFY");
}

void SupervisedUserVerificationPage::CommandReceived(
    const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }
  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  switch (cmd) {
    case security_interstitials::CMD_OPEN_LOGIN:
      controller()->OpenUrlInCurrentTab(signin::GetChromeReauthURL(
          {.email = email_to_reauth_, .continue_url = request_url_}));
      break;
    case security_interstitials::CMD_DONT_PROCEED:
    case security_interstitials::CMD_OPEN_HELP_CENTER:
    case security_interstitials::CMD_PROCEED:
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_SHOW_MORE_SECTION:
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
    case security_interstitials::CMD_RELOAD:
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Not supported by the verification page.
      NOTREACHED_IN_MIGRATION() << "Unsupported command: " << command;
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
    default:
      NOTREACHED_NORETURN();
  }
}
