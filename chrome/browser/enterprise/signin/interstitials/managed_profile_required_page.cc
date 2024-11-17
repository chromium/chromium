// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/interstitials/managed_profile_required_page.h"

#include <utility>

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/signin/managed_profile_required_navigation_throttle.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/grit/branded_strings.h"
#include "components/enterprise/connectors/core/enterprise_interstitial_util.h"
#include "components/grit/components_resources.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_interstitials/core/metrics_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "components/security_interstitials/core/urls.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using security_interstitials::MetricsHelper;

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    ManagedProfileRequiredPage::kTypeForTesting =
        &ManagedProfileRequiredPage::kTypeForTesting;

ManagedProfileRequiredPage::ManagedProfileRequiredPage(
    content::WebContents* web_contents,
    const GURL& request_url,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)) {
  controller()->metrics_helper()->RecordUserDecision(MetricsHelper::SHOW);
  controller()->metrics_helper()->RecordUserInteraction(
      MetricsHelper::TOTAL_VISITS);
  controller()->metrics_helper()->RecordUserDecision(
      security_interstitials::MetricsHelper::PROCEEDING_DISABLED);
}

ManagedProfileRequiredPage::~ManagedProfileRequiredPage() = default;

security_interstitials::SecurityInterstitialPage::TypeID
ManagedProfileRequiredPage::GetTypeForTesting() {
  return ManagedProfileRequiredPage::kTypeForTesting;
}

void ManagedProfileRequiredPage::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  PopulateStringsForSharedHTML(load_time_data);
  load_time_data.Set(
      "tabTitle",
      l10n_util::GetStringUTF16(IDS_MANAGED_PROFILE_INTERSTITIAL_TAB_TITLE));
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringUTF16(
                         IDS_MANAGED_PROFILE_INTERSTITIAL_PRIMARY_PARAGRAPH));

  load_time_data.Set("heading", l10n_util::GetStringUTF16(
                                    IDS_MANAGED_PROFILE_INTERSTITIAL_HEADING));
  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(IDS_APP_CONTINUE));
}

void ManagedProfileRequiredPage::OnInterstitialClosing() {}

void ManagedProfileRequiredPage::CommandReceived(const std::string& command) {
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
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      if (ManagedProfileRequiredNavigationThrottle::IsBlockingNavigations(
              web_contents()->GetBrowserContext())) {
        ManagedProfileRequiredNavigationThrottle::ShowBlockedWindow(
            web_contents()->GetBrowserContext());
      } else {
        controller()->metrics_helper()->RecordUserDecision(
            MetricsHelper::DONT_PROCEED);
        controller()->Reload();
      }
#else
      controller()->metrics_helper()->RecordUserDecision(
          MetricsHelper::DONT_PROCEED);
      controller()->Reload();
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      break;
    case security_interstitials::CMD_PROCEED:
    case security_interstitials::CMD_OPEN_HELP_CENTER:
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

base::Value::Dict ManagedProfileRequiredPage::GetLoadTimeDataForTesting() {
  base::Value::Dict load_time_data;
  PopulateInterstitialStrings(load_time_data);
  return load_time_data;
}

void ManagedProfileRequiredPage::PopulateStringsForSharedHTML(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("managed-profile-required", true);
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);

  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("optInLink", "");
  load_time_data.Set("enhancedProtectionMessage", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");

  load_time_data.Set("type", "MANAGED_PROFILE_REQUIRED");
}
