// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/lookalike_url_interstitial_page.h"

#include <utility>

#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/l10n/l10n_util.h"

using security_interstitials::MetricsHelper;

// static
const content::InterstitialPageDelegate::TypeID
    LookalikeUrlInterstitialPage::kTypeForTesting =
        &LookalikeUrlInterstitialPage::kTypeForTesting;

LookalikeUrlInterstitialPage::LookalikeUrlInterstitialPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    ukm::SourceId source_id,
    MatchType match_type,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      source_id_(source_id),
      match_type_(match_type) {
  controller()->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller()->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
}

LookalikeUrlInterstitialPage::~LookalikeUrlInterstitialPage() {}

void LookalikeUrlInterstitialPage::ReportUkmIfNeeded(UserAction action) {
  // We rely on the saved SourceId because deconstruction happens after the next
  // navigation occurs, so web contents points to the new destination.
  if (source_id_ != ukm::kInvalidSourceId) {
    RecordUkmEvent(source_id_, match_type_, action);
    source_id_ = ukm::kInvalidSourceId;
  }
}

// static
void LookalikeUrlInterstitialPage::RecordUkmEvent(
    ukm::SourceId source_id,
    LookalikeUrlInterstitialPage::MatchType match_type,
    LookalikeUrlInterstitialPage::UserAction user_action) {
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  CHECK(ukm_recorder);

  ukm::builders::LookalikeUrl_NavigationSuggestion(source_id)
      .SetMatchType(static_cast<int>(match_type))
      .SetUserAction(static_cast<int>(user_action))
      .Record(ukm_recorder);
}

content::InterstitialPageDelegate::TypeID
LookalikeUrlInterstitialPage::GetTypeForTesting() {
  return LookalikeUrlInterstitialPage::kTypeForTesting;
}

bool LookalikeUrlInterstitialPage::ShouldCreateNewNavigation() const {
  return true;
}

void LookalikeUrlInterstitialPage::PopulateInterstitialStrings(
    base::DictionaryValue* load_time_data) {
  CHECK(load_time_data);

  PopulateStringsForSharedHTML(load_time_data);
  security_interstitials::common_string_util::PopulateDarkModeDisplaySetting(
      load_time_data);

  const base::string16 hostname =
      security_interstitials::common_string_util::GetFormattedHostName(
          request_url());
  load_time_data->SetString("tabTitle", l10n_util::GetStringFUTF16(
                                            IDS_LOOKALIKE_URL_TITLE, hostname));
  load_time_data->SetString(
      "heading",
      l10n_util::GetStringFUTF16(IDS_LOOKALIKE_URL_HEADING, hostname));
  load_time_data->SetString(
      "primaryParagraph",
      l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH));
  load_time_data->SetString(
      "proceedButtonText", l10n_util::GetStringUTF16(IDS_LOOKALIKE_URL_IGNORE));
  load_time_data->SetString(
      "primaryButtonText",
      l10n_util::GetStringFUTF16(IDS_LOOKALIKE_URL_CONTINUE, hostname));
}

void LookalikeUrlInterstitialPage::OnInterstitialClosing() {
  ReportUkmIfNeeded(UserAction::kCloseOrBack);
}

bool LookalikeUrlInterstitialPage::ShouldDisplayURL() const {
  return false;
}

// This handles the commands sent from the interstitial JavaScript.
void LookalikeUrlInterstitialPage::CommandReceived(const std::string& command) {
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
      ReportUkmIfNeeded(UserAction::kAcceptSuggestion);
      controller()->GoBack();
      break;
    case security_interstitials::CMD_PROCEED:
      controller()->metrics_helper()->RecordUserDecision(
          MetricsHelper::PROCEED);
      ReportUkmIfNeeded(UserAction::kClickThrough);
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
      NOTREACHED() << "Unsupported command: " << command;
      break;
    case security_interstitials::CMD_ERROR:
    case security_interstitials::CMD_TEXT_FOUND:
    case security_interstitials::CMD_TEXT_NOT_FOUND:
      // Commands are for testing.
      break;
  }
}

int LookalikeUrlInterstitialPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

void LookalikeUrlInterstitialPage::PopulateStringsForSharedHTML(
    base::DictionaryValue* load_time_data) {
  load_time_data->SetBoolean("lookalike_url", true);
  load_time_data->SetBoolean("overridable", false);
  load_time_data->SetBoolean("hide_primary_button", false);
  load_time_data->SetBoolean("show_recurrent_error_paragraph", false);

  load_time_data->SetString("recurrentErrorParagraph", "");
  load_time_data->SetString("openDetails", "");
  load_time_data->SetString("explanationParagraph", "");
  load_time_data->SetString("finalParagraph", "");

  load_time_data->SetString("type", "LOOKALIKE");
}
