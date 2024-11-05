// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_verification_page_youtube.h"

#include <utility>

#include "components/grit/components_resources.h"
#include "components/security_interstitials/content/security_interstitial_controller_client.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/l10n/l10n_util.h"

// static
const security_interstitials::SecurityInterstitialPage::TypeID
    SupervisedUserVerificationPageForYouTube::kTypeForTesting =
        &SupervisedUserVerificationPageForYouTube::kTypeForTesting;

SupervisedUserVerificationPageForYouTube::
    SupervisedUserVerificationPageForYouTube(
        content::WebContents* web_contents,
        const std::string& email_to_reauth,
        const GURL& request_url,
        supervised_user::ChildAccountService* child_account_service,
        ukm::SourceId source_id,
        std::unique_ptr<
            security_interstitials::SecurityInterstitialControllerClient>
            controller_client,
        bool is_main_frame)
    : SupervisedUserVerificationPage(web_contents,
                                     email_to_reauth,
                                     request_url,
                                     child_account_service,
                                     std::move(controller_client)),
      source_id_(source_id),
      is_main_frame_(is_main_frame) {
  // Demo interstitials are created without `child_account_service` and should
  // not have metrics recorded.
  if (child_account_service) {
    RecordReauthStatusMetrics(Status::SHOWN);
  }
}

SupervisedUserVerificationPageForYouTube::
    ~SupervisedUserVerificationPageForYouTube() {
  if (IsReauthCompleted()) {
    RecordReauthStatusMetrics(Status::REAUTH_COMPLETED);
  }
}

security_interstitials::SecurityInterstitialPage::TypeID
SupervisedUserVerificationPageForYouTube::GetTypeForTesting() {
  return SupervisedUserVerificationPageForYouTube::kTypeForTesting;
}

void SupervisedUserVerificationPageForYouTube::PopulateInterstitialStrings(
    base::Value::Dict& load_time_data) {
  if (is_main_frame_) {
    load_time_data.Set("type", "SUPERVISED_USER_VERIFY");
  } else {
    load_time_data.Set("type", "SUPERVISED_USER_VERIFY_SUBFRAME");
  }

  PopulateCommonStrings(load_time_data);

  load_time_data.Set(
      "tabTitle",
      l10n_util::GetStringUTF16(IDS_SUPERVISED_USER_VERIFY_PAGE_TAB_TITLE));
  load_time_data.Set(
      "heading",
      is_main_frame_
          ? l10n_util::GetStringUTF16(
                IDS_SUPERVISED_USER_VERIFY_PAGE_PRIMARY_HEADING)
          : l10n_util::GetStringUTF16(
                IDS_SUPERVISED_USER_VERIFY_PAGE_SUBFRAME_YOUTUBE_HEADING));
  load_time_data.Set("primaryParagraph",
                     l10n_util::GetStringUTF16(
                         IDS_SUPERVISED_USER_VERIFY_PAGE_PRIMARY_PARAGRAPH));
  load_time_data.Set("primaryButtonText",
                     l10n_util::GetStringUTF16(
                         IDS_SUPERVISED_USER_VERIFY_PAGE_PRIMARY_BUTTON));
}

void SupervisedUserVerificationPageForYouTube::RecordReauthStatusMetrics(
    Status status) {
  if (!is_main_frame_) {
    // Do not record metrics for subframe interstitials.
    return;
  }

  auto builder =
      ukm::builders::FamilyLinkUser_ReauthenticationInterstitial(source_id_);
  switch (status) {
    case Status::SHOWN:
      builder.SetInterstitialShown(true);
      break;
    case Status::REAUTH_STARTED:
      builder.SetReauthenticationStarted(true);
      break;
    case Status::REAUTH_COMPLETED:
      builder.SetReauthenticationCompleted(true);
      break;
    default:
      NOTREACHED();
  }
  builder.Record(ukm::UkmRecorder::Get());
}
