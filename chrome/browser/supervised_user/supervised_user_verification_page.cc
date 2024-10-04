// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_verification_page.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/l10n/l10n_util.h"

// static
bool SupervisedUserVerificationPage::ShouldShowPage(
    const supervised_user::ChildAccountService& child_account_service) {
  switch (child_account_service.GetGoogleAuthState()) {
    case supervised_user::ChildAccountService::AuthState::NOT_AUTHENTICATED:
    case supervised_user::ChildAccountService::AuthState::AUTHENTICATED:
      // The user is fully signed out or fully signed in. Don't show the
      // interstitial.
      return false;

    case supervised_user::ChildAccountService::AuthState::PENDING:
    case supervised_user::ChildAccountService::AuthState::
        TRANSIENT_MOVING_TO_AUTHENTICATED:
      // The user is in a stable pending state, or a transient state. Show the
      // interstitial, as a parent approval request or YouTube visit would not
      // be successful with the correct behavior.
      //
      // In the transient case, an update to AUTHENTICATED state may shortly
      // follow, which will trigger this interstitial to be refreshed.
      return true;
  }
}

SupervisedUserVerificationPage::SupervisedUserVerificationPage(
    content::WebContents* web_contents,
    const std::string& email_to_reauth,
    const GURL& request_url,
    supervised_user::ChildAccountService* child_account_service,
    std::unique_ptr<
        security_interstitials::SecurityInterstitialControllerClient>
        controller_client)
    : security_interstitials::SecurityInterstitialPage(
          web_contents,
          request_url,
          std::move(controller_client)),
      email_to_reauth_(email_to_reauth),
      request_url_(request_url),
      sign_in_continue_url_(GaiaUrls::GetInstance()->blank_page_url()),
      reauth_url_(signin::GetChromeReauthURL(
          {.email = email_to_reauth_, .continue_url = sign_in_continue_url_})),
      child_account_service_(child_account_service) {
  if (child_account_service_) {
    // Reloads the interstitial to continue navigation once the supervised user
    // is authenticated. Also closes the sign-in tabs opened by this
    // interstitial.
    google_auth_state_subscription_ =
        child_account_service_->ObserveGoogleAuthState(base::BindRepeating(
            &SupervisedUserVerificationPage::OnGoogleAuthStateUpdate,
            weak_factory_.GetWeakPtr()));
  }
}

SupervisedUserVerificationPage::~SupervisedUserVerificationPage() = default;

void SupervisedUserVerificationPage::CloseSignInTabs() {
  while (!signin_tabs_handle_id_list_.empty()) {
    auto tab_handle_id = signin_tabs_handle_id_list_.front();
    signin_tabs_handle_id_list_.pop_front();
    // Obtains the tab associated with the unique tab handle id. A tab pointer
    // is only returned if the tab is still valid.
    auto* tab_interface = tabs::TabInterface::MaybeGetFromHandle(tab_handle_id);
    if (!tab_interface) {
      continue;
    }
    // Check both visible url and last committed url, as the last committed url
    // can be empty (if the navigation of the sign-in tab has not yet
    // committed).
    // Only urls that are known to be part of the sign-in flow will be closed,
    // the rest will be left open as the user might have navigated elsewhere.
    if (!IsSignInUrl(tab_interface->GetContents()->GetLastCommittedURL()) &&
        !IsSignInUrl(tab_interface->GetContents()->GetVisibleURL())) {
      continue;
    }
    tab_interface->Close();
    // TODO(b/364546097): Add metrics for the cases where we skip the tab
    // closure.
    }
  // TODO(b/364546097): Ideally focus the last visited tab (before the sign-in
  // page), before closing the sign-in tabs.
}

bool SupervisedUserVerificationPage::IsSignInUrl(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }
  return url.host_piece() == reauth_url_.host_piece() ||
         url.host_piece() == sign_in_continue_url_.host_piece();
}

void SupervisedUserVerificationPage::OnGoogleAuthStateUpdate() {
  // This callback doesn't guarantee that the state has changed, or that it has
  // transitioned to fully signed in.
  // If we're still in a state where we should be showing this interstitial,
  // drop out.
  CHECK(child_account_service_);
  if (ShouldShowPage(*child_account_service_)) {
    return;
  }

  // Re-authentication metrics will be recorded in the destructor, since this
  // method could be invoked more than once.
  is_reauth_completed_ = true;
  if (base::FeatureList::IsEnabled(
          supervised_user::kCloseSignTabsFromReauthenticationInterstitial)) {
    CloseSignInTabs();
  }
  controller()->Reload();
}

void SupervisedUserVerificationPage::OnInterstitialClosing() {}

int SupervisedUserVerificationPage::GetHTMLTemplateId() {
  return IDR_SECURITY_INTERSTITIAL_HTML;
}

void SupervisedUserVerificationPage::PopulateCommonStrings(
    base::Value::Dict& load_time_data) {
  load_time_data.Set("overridable", false);
  load_time_data.Set("hide_primary_button", false);
  load_time_data.Set("show_recurrent_error_paragraph", false);

  load_time_data.Set("recurrentErrorParagraph", "");
  load_time_data.Set("openDetails", "");
  load_time_data.Set("explanationParagraph", "");
  load_time_data.Set("finalParagraph", "");
  load_time_data.Set("optInLink", l10n_util::GetStringUTF16(
                                      IDS_SAFE_BROWSING_SCOUT_REPORTING_AGREE));
  load_time_data.Set(
      "enhancedProtectionMessage",
      l10n_util::GetStringUTF16(IDS_SAFE_BROWSING_ENHANCED_PROTECTION_MESSAGE));
}

bool SupervisedUserVerificationPage::IsReauthCompleted() {
  return is_reauth_completed_;
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
    case security_interstitials::CMD_OPEN_LOGIN: {
      RecordReauthStatusMetrics(Status::REAUTH_STARTED);
      content::OpenURLParams params(reauth_url_, content::Referrer(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    ui::PAGE_TRANSITION_LINK, false);
      auto* signin_web_contents =
          SecurityInterstitialPage::web_contents()->OpenURL(
              params, /*navigation_handle_callback=*/{});
      if (base::FeatureList::IsEnabled(
              supervised_user::
                  kCloseSignTabsFromReauthenticationInterstitial) &&
          signin_web_contents) {
        tabs::TabInterface* tab_interface =
            tabs::TabInterface::GetFromContents(signin_web_contents);
        signin_tabs_handle_id_list_.emplace_back(tab_interface->GetTabHandle());
      }
      break;
    }
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
      NOTREACHED();
  }
}
