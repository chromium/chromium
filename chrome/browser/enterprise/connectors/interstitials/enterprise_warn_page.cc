// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/interstitials/enterprise_warn_page.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/connectors/core/enterprise_interstitial_util.h"
#include "components/grit/components_resources.h"
#include "components/safe_browsing/content/browser/base_ui_manager.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
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
    EnterpriseWarnPage::kTypeForTesting = &EnterpriseWarnPage::kTypeForTesting;

EnterpriseWarnPage::EnterpriseWarnPage(
    safe_browsing::BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const GURL& request_url,
    const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList&
        unsafe_resources,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      ui_manager_(ui_manager),
      unsafe_resources_(unsafe_resources) {
  controller()->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller()->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
}

EnterpriseWarnPage::~EnterpriseWarnPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
EnterpriseWarnPage::GetTypeForTesting() {
  return EnterpriseWarnPage::kTypeForTesting;
}

void EnterpriseWarnPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  PopulateStringsForSharedHTML(load_time_data);
  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_ENTERPRISE_WARN_TITLE));
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));

  load_time_data.Set("heading",
                     l10n_util::GetStringUTF16(IDS_ENTERPRISE_WARN_HEADING));

  std::u16string custom_message =
      enterprise_connectors::GetUrlFilteringCustomMessage(unsafe_resources_);
  if (!custom_message.empty()) {
    load_time_data.Set("primaryParagraph",
                       l10n_util::GetStringFUTF16(
                           IDS_ENTERPRISE_WARN_PRIMARY_PARAGRAPH_CUSTOM_MESSAGE,
                           custom_message));
  } else {
    load_time_data.Set(
        "primaryParagraph",
        l10n_util::GetStringFUTF16(
            IDS_ENTERPRISE_WARN_PRIMARY_PARAGRAPH,
            security_interstitials::common_string_util::GetFormattedHostName(
                request_url()),
            l10n_util::GetStringUTF16(
                IDS_ENTERPRISE_INTERSTITIALS_LEARN_MORE_ACCCESSIBILITY_TEXT)));
  }

  load_time_data.Set(
      "proceedButtonText",
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_WARN_CONTINUE_TO_SITE));
  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(IDS_ENTERPRISE_WARN_GO_BACK));
}

void EnterpriseWarnPage::OnInterstitialClosing() {}

void EnterpriseWarnPage::CommandReceived(const std::string& command) {
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
    case security_interstitials::CMD_PROCEED: {
      controller()->metrics_helper()->RecordUserDecision(
          MetricsHelper::PROCEED);
      // Add to allowlist.
      ui_manager_->OnBlockingPageDone(unsafe_resources_, /*proceed=*/true,
                                      web_contents(), request_url(),
                                      /*showed_interstitial=*/true);
      controller()->Proceed();
      break;
    }
    case security_interstitials::CMD_OPEN_HELP_CENTER:
      controller()->OpenUrlInNewForegroundTab(
          GURL(security_interstitials::kEnterpriseInterstitialHelpLink));
      break;
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
      // Not supported by the URL warning page.
      NOTREACHED_IN_MIGRATION() << "Unsupported command: " << command;
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

int EnterpriseWarnPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

std::string EnterpriseWarnPage::GetCustomMessageForTesting() {
  base::Value::Dict load_time_data;
  PopulateInterstitialStrings(load_time_data);
  std::string custom_message = *load_time_data.FindString("primaryParagraph");
  return custom_message;
}

void EnterpriseWarnPage::PopulateStringsForSharedHTML(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("enterprise-warn", true);
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);
  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
  load_time_data.Set("primaryButtonText", "");
  load_time_data.Set("type", "ENTERPRISE_WARN");
}
