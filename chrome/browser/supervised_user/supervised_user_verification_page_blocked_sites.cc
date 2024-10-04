// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_verification_page_blocked_sites.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
constexpr char kBlockedSiteVerifyItsYouInterstitialStateHistogramName[] =
    "FamilyLinkUser.BlockedSiteVerifyItsYouInterstitialState";
}  // namespace

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    SupervisedUserVerificationPageForBlockedSites::kTypeForTesting =
        &SupervisedUserVerificationPageForBlockedSites::kTypeForTesting;

SupervisedUserVerificationPageForBlockedSites::
    SupervisedUserVerificationPageForBlockedSites(
        content::WebContents* web_contents,
        const std::string& email_to_reauth,
        const GURL& request_url,
        supervised_user::ChildAccountService* child_account_service,
        std::unique_ptr<
            security_interstitials::SecurityInterstitialControllerClient>
            controller_client,
        supervised_user::FilteringBehaviorReason block_reason,
        bool is_main_frame,
        bool has_second_custodian)
    : SupervisedUserVerificationPage(web_contents,
                                     email_to_reauth,
                                     request_url,
                                     child_account_service,
                                     std::move(controller_client)),
      block_reason_(block_reason),
      is_main_frame_(is_main_frame),
      has_second_custodian_(has_second_custodian) {
  // Demo interstitials are created without `child_account_service` and should
  // not have metrics recorded.
  if (child_account_service) {
    RecordReauthStatusMetrics(Status::SHOWN);
  }
}

SupervisedUserVerificationPageForBlockedSites::
    ~SupervisedUserVerificationPageForBlockedSites() {
  if (IsReauthCompleted()) {
    RecordReauthStatusMetrics(Status::REAUTH_COMPLETED);
  }
}

security_interstitials::SecurityInterstitialPage::TypeID
SupervisedUserVerificationPageForBlockedSites::GetTypeForTesting() {
  return SupervisedUserVerificationPageForBlockedSites::kTypeForTesting;
}

void SupervisedUserVerificationPageForBlockedSites::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  if (is_main_frame_) {
    load_time_data.Set("type", "SUPERVISED_USER_VERIFY");
  } else {
    load_time_data.Set("type", "SUPERVISED_USER_VERIFY_SUBFRAME");
  }

  PopulateCommonStrings(load_time_data);

  load_time_data.Set("tabTitle",
                     l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));
  load_time_data.Set(
      "heading",
      is_main_frame_
          ? l10n_util::GetStringUTF16(IDS_CHILD_BLOCK_INTERSTITIAL_HEADER)
          : l10n_util::GetStringUTF16(
                IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_BLOCKED_SITE_HEADING));
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringUTF16(
                         IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_NOT_SIGNED_IN));
  load_time_data.Set("show_blocked_site_message", true);
  load_time_data.Set("blockedSiteMessageHeader",
                     l10n_util::GetStringUTF8(IDS_GENERIC_SITE_BLOCK_HEADER));
  load_time_data.Set("blockedSiteMessageReason",
                     l10n_util::GetStringUTF8(GetBlockMessageReasonId()));
  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(
                         IDS_SUPERVISED_USER_VERIFY_PAGE_PRIMARY_BUTTON));
}

void SupervisedUserVerificationPageForBlockedSites::RecordReauthStatusMetrics(
    Status status) {
  if (!is_main_frame_) {
    // Do not record metrics for subframe interstitials.
    return;
  }

  auto state =
      FamilyLinkUserReauthenticationInterstitialState::kInterstitialShown;
  switch (status) {
    case Status::SHOWN:
      break;
    case Status::REAUTH_STARTED:
      state = FamilyLinkUserReauthenticationInterstitialState::
          kReauthenticationStarted;
      break;
    case Status::REAUTH_COMPLETED:
      state = FamilyLinkUserReauthenticationInterstitialState::
          kReauthenticationCompleted;
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(
      kBlockedSiteVerifyItsYouInterstitialStateHistogramName, state);
}

int SupervisedUserVerificationPageForBlockedSites::GetBlockMessageReasonId() {
  switch (block_reason_) {
    case supervised_user::FilteringBehaviorReason::DEFAULT:
      return has_second_custodian_
                 ? IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT
                 : IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT;
    case supervised_user::FilteringBehaviorReason::ASYNC_CHECKER:
      return IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES;
    case supervised_user::FilteringBehaviorReason::MANUAL:
      return has_second_custodian_
                 ? IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT
                 : IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT;
    default:
      NOTREACHED_NORETURN();
  }
}
