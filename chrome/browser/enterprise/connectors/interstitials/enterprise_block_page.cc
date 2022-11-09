// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_page.h"

#include <utility>

#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/urls.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"

using security_interstitials::MetricsHelper;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    EnterpriseBlockPage::kTypeForTesting =
        &EnterpriseBlockPage::kTypeForTesting;

EnterpriseBlockPage::EnterpriseBlockPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)) {}

EnterpriseBlockPage::~EnterpriseBlockPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
EnterpriseBlockPage::GetTypeForTesting() {
  return EnterpriseBlockPage::kTypeForTesting;
}

void EnterpriseBlockPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  PopulateStringsForSharedHTML(load_time_data);
  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_TITLE));
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));

  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_HEADING));
  load_time_data.Set(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_ENTERPRISE_BLOCK_PRIMARY_PARAGRAPH,
          security_interstitials::common_string_util::GetFormattedHostName(
              request_url())));
  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(IDS_ENTERPRISE_BLOCK_GO_BACK));
}

void EnterpriseBlockPage::OnInterstitialClosing() {}

void EnterpriseBlockPage::CommandReceived(const std::string& command) {
  if (command == "\"pageLoadComplete\"") {
    // content::WaitForRenderFrameReady sends this message when the page
    // load completes. Ignore it.
    return;
  }

  int cmd = 0;
  bool retval = base::StringToInt(command, &cmd);
  DCHECK(retval);

  switch (cmd) {
    case security_interstitials::CMD_DONT_PROCEED:
      controller()->GoBack();
      break;
    case security_interstitials::CMD_OPEN_HELP_CENTER:
      controller()->OpenUrlInNewForegroundTab(
          GURL(security_interstitials::kEnterpriseInterstitialHelpLink));
      break;
    case security_interstitials::CMD_PROCEED:
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_SHOW_MORE_SECTION:
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
    case security_interstitials::CMD_RELOAD:
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
    case security_interstitials::CMD_OPEN_LOGIN:
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Not supported by the URL blocking page.
      NOTREACHED() << "Unsupported command: " << command;
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

int EnterpriseBlockPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

void EnterpriseBlockPage::PopulateStringsForSharedHTML(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("enterprise-block", true);
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);

  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
  load_time_data.Set("primaryButtonText", "");

  load_time_data.Set("type", "ENTERPRISE_BLOCK");
}
