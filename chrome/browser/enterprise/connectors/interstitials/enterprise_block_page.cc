// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/interstitials/enterprise_block_page.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/connectors/core/enterprise_interstitial_util.h"
#include "components/grit/components_resources.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/urls.h"
#include "components/strings/grit/components_strings.h"
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
    const safe_browsing::SafeBrowsingBlockingPage::UnsafeResourceList&
        unsafe_resources,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      unsafe_resources_(unsafe_resources) {
  controller()->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller()->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
  controller()->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::PROCEEDING_DISABLED);
}

EnterpriseBlockPage::~EnterpriseBlockPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
EnterpriseBlockPage::GetTypeForTesting() {
  return EnterpriseBlockPage::kTypeForTesting;
}

enterprise_connectors::EnterpriseInterstitialBase::Type
EnterpriseBlockPage::type() const {
  return Type::kBlock;
}

const std::vector<security_interstitials::UnsafeResource>&
EnterpriseBlockPage::unsafe_resources() const {
  return unsafe_resources_;
}

GURL EnterpriseBlockPage::request_url() const {
  return security_interstitials::SecurityInterstitialPage::request_url();
}

void EnterpriseBlockPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  PopulateStrings(load_time_data);
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
      controller()->metrics_helper()->RecordUserDecision(
          MetricsHelper::DONT_PROCEED);
      controller()->GoBack();
      break;
    case security_interstitials::CMD_OPEN_HELP_CENTER:
      controller()->metrics_helper()->RecordUserInteraction(
          MetricsHelper::SHOW_LEARN_MORE);
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

std::string EnterpriseBlockPage::GetCustomMessageForTesting() {
  base::Value::Dict load_time_data;
  PopulateInterstitialStrings(load_time_data);
  std::string custom_message = *load_time_data.FindString("primaryParagraph");
  return custom_message;
}

