// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_blocking_page.h"

#include <utility>

#include "chrome/common/webui_url_constants.h"
#include "components/grit/components_resources.h"
#include "components/lookalikes/core/lookalike_url_ui_util.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

using lookalikes::LookalikeUrlBlockingPageUserAction;
using lookalikes::LookalikeUrlMatchType;
using security_interstitials::MetricsHelper;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    LookalikeUrlBlockingPage::kTypeForTesting =
        &LookalikeUrlBlockingPage::kTypeForTesting;

LookalikeUrlBlockingPage::LookalikeUrlBlockingPage(
    content::WebContents* web_contents,
    const GURL& safe_url,
    const GURL& request_url,
    ukm::SourceId source_id,
    LookalikeUrlMatchType match_type,
    bool is_signed_exchange,
    bool triggered_by_initial_url,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      safe_url_(safe_url),
      source_id_(source_id),
      match_type_(match_type),
      is_signed_exchange_(is_signed_exchange),
      triggered_by_initial_url_(triggered_by_initial_url) {
  controller()->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller()->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
}

LookalikeUrlBlockingPage::~LookalikeUrlBlockingPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
LookalikeUrlBlockingPage::GetTypeForTesting() {
  return LookalikeUrlBlockingPage::kTypeForTesting;
}

void LookalikeUrlBlockingPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  lookalikes::PopulateLookalikeUrlBlockingPageStrings(load_time_data, safe_url_,
                                                      request_url());
}

void LookalikeUrlBlockingPage::OnInterstitialClosing() {
  lookalikes::ReportUkmForLookalikeUrlBlockingPageIfNeeded(
      source_id_, match_type_, LookalikeUrlBlockingPageUserAction::kCloseOrBack,
      triggered_by_initial_url_);
}

bool LookalikeUrlBlockingPage::ShouldDisplayURL() const {
  return false;
}

// This handles the commands sent from the interstitial JavaScript.
void LookalikeUrlBlockingPage::CommandReceived(const std::string& command) {
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
      controller()->metrics_helper()->RecordUserDecision(
          MetricsHelper::DONT_PROCEED);
      lookalikes::ReportUkmForLookalikeUrlBlockingPageIfNeeded(
          source_id_, match_type_,
          LookalikeUrlBlockingPageUserAction::kAcceptSuggestion,
          triggered_by_initial_url_);
      // If the interstitial doesn't have a suggested URL (e.g. punycode
      // interstitial), simply open the new tab page.
      if (!safe_url_.is_valid()) {
        controller()->OpenUrlInCurrentTab(GURL(chrome::kChromeUINewTabURL));
      } else {
        controller()->GoBack();
      }
      break;
    case security_interstitials::CMD_PROCEED:
      controller()->metrics_helper()->RecordUserDecision(
          MetricsHelper::PROCEED);
      lookalikes::ReportUkmForLookalikeUrlBlockingPageIfNeeded(
          source_id_, match_type_,
          LookalikeUrlBlockingPageUserAction::kClickThrough,
          triggered_by_initial_url_);
      controller()->Proceed();
      break;
    case security_interstitials::CMD_DO_REPORT:
    case security_interstitials::CMD_DONT_REPORT:
    case security_interstitials::CMD_SHOW_MORE_SECTION:
    case security_interstitials::CMD_OPEN_DATE_SETTINGS:
    case security_interstitials::CMD_OPEN_REPORTING_PRIVACY:
    case security_interstitials::CMD_OPEN_WHITEPAPER:
    case security_interstitials::CMD_OPEN_HELP_CENTER:
    case security_interstitials::CMD_RELOAD:
    case security_interstitials::CMD_OPEN_DIAGNOSTIC:
    case security_interstitials::CMD_OPEN_LOGIN:
    case security_interstitials::CMD_REPORT_PHISHING_ERROR:
      // Not supported by the lookalike URL warning page.
      NOTREACHED_IN_MIGRATION() << "Unsupported command: " << command;
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

int LookalikeUrlBlockingPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}
