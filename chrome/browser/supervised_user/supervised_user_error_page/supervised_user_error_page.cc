// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"

#include <string>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_features/supervised_user_features.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

namespace supervised_user_error_page {

namespace {

static const int kAvatarSize1x = 36;
static const int kAvatarSize2x = 72;

bool ReasonIsAutomatic(FilteringBehaviorReason reason) {
  return reason == ASYNC_CHECKER || reason == DENYLIST;
}

std::string BuildAvatarImageUrl(const std::string& url, int size) {
  GURL gurl(url);
  if (!gurl.is_valid())
    return url;

  GURL to_return = signin::GetAvatarImageURLWithOptions(
      gurl, size, false /* no_silhouette */);
  return to_return.spec();
}

}  //  namespace

int GetBlockMessageID(FilteringBehaviorReason reason, bool single_parent) {
  switch (reason) {
    case DEFAULT:
      return single_parent ? IDS_CHILD_BLOCK_MESSAGE_DEFAULT_SINGLE_PARENT
                           : IDS_CHILD_BLOCK_MESSAGE_DEFAULT_MULTI_PARENT;
    case DENYLIST:
    case ASYNC_CHECKER:
      return IDS_SUPERVISED_USER_BLOCK_MESSAGE_SAFE_SITES;
    case ALLOWLIST:
      NOTREACHED();
      break;
    case MANUAL:
      return single_parent ? IDS_CHILD_BLOCK_MESSAGE_MANUAL_SINGLE_PARENT
                           : IDS_CHILD_BLOCK_MESSAGE_MANUAL_MULTI_PARENT;
    case NOT_SIGNED_IN:
      return IDS_SUPERVISED_USER_NOT_SIGNED_IN;
  }
  NOTREACHED();
  return 0;
}

std::string BuildHtml(bool allow_access_requests,
                      const std::string& profile_image_url,
                      const std::string& profile_image_url2,
                      const std::string& custodian,
                      const std::string& custodian_email,
                      const std::string& second_custodian,
                      const std::string& second_custodian_email,
                      FilteringBehaviorReason reason,
                      const std::string& app_locale,
                      bool already_sent_remote_request,
                      bool is_main_frame) {
  base::DictionaryValue strings;
  strings.SetString("blockPageTitle",
                    l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_TITLE));
  strings.SetBoolean("allowAccessRequests", allow_access_requests);
  strings.SetString("avatarURL1x",
                    BuildAvatarImageUrl(profile_image_url, kAvatarSize1x));
  strings.SetString("avatarURL2x",
                    BuildAvatarImageUrl(profile_image_url, kAvatarSize2x));
  strings.SetString("secondAvatarURL1x",
                    BuildAvatarImageUrl(profile_image_url2, kAvatarSize1x));
  strings.SetString("secondAvatarURL2x",
                    BuildAvatarImageUrl(profile_image_url2, kAvatarSize2x));
  strings.SetString("custodianName", custodian);
  strings.SetString("custodianEmail", custodian_email);
  strings.SetString("secondCustodianName", second_custodian);
  strings.SetString("secondCustodianEmail", second_custodian_email);
  strings.SetBoolean("alreadySentRemoteRequest", already_sent_remote_request);
  strings.SetBoolean("isMainFrame", is_main_frame);
  bool web_filter_interstitial_refresh_enabled =
      supervised_users::IsWebFilterInterstitialRefreshEnabled();
  bool local_web_approvals_enabled =
      supervised_users::IsLocalWebApprovalsEnabled();
  strings.SetBoolean("isWebFilterInterstitialRefreshEnabled",
                     web_filter_interstitial_refresh_enabled);
  strings.SetBoolean("isLocalWebApprovalsEnabled", local_web_approvals_enabled);
  bool is_automatically_blocked = ReasonIsAutomatic(reason);

  std::u16string custodian16 = base::UTF8ToUTF16(custodian);
  std::u16string block_header;
  std::u16string block_message;
  if (reason == FilteringBehaviorReason::NOT_SIGNED_IN) {
    block_header =
        l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_HEADER_NOT_SIGNED_IN);
  } else if (allow_access_requests) {
    block_header =
        l10n_util::GetStringUTF16(IDS_CHILD_BLOCK_INTERSTITIAL_HEADER);
    block_message = l10n_util::GetStringUTF16(
        web_filter_interstitial_refresh_enabled && is_automatically_blocked
            ? IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE_SAFE_SITES_BLOCKED
            : IDS_CHILD_BLOCK_INTERSTITIAL_MESSAGE);
  } else {
    block_header = l10n_util::GetStringUTF16(
        IDS_BLOCK_INTERSTITIAL_HEADER_ACCESS_REQUESTS_DISABLED);
  }
  strings.SetString("blockPageHeader", block_header);
  strings.SetString("blockPageMessage", block_message);
  strings.SetString("blockReasonMessage",
                    l10n_util::GetStringUTF16(
                        GetBlockMessageID(reason, second_custodian.empty())));
  strings.SetString("blockReasonHeader", l10n_util::GetStringUTF16(
                                             IDS_SUPERVISED_USER_BLOCK_HEADER));

  strings.SetBoolean("showFeedbackLink", is_automatically_blocked);
  strings.SetString("feedbackLink", l10n_util::GetStringUTF16(
                                        IDS_BLOCK_INTERSTITIAL_SEND_FEEDBACK));
  if (web_filter_interstitial_refresh_enabled) {
    strings.SetString(
        "remoteApprovalsButton",
        l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_SEND_MESSAGE_BUTTON));
    strings.SetString("backButton",
                      l10n_util::GetStringUTF16(IDS_REQUEST_SENT_OK));
  } else {
    strings.SetString("remoteApprovalsButton",
                      l10n_util::GetStringUTF16(
                          IDS_BLOCK_INTERSTITIAL_REQUEST_ACCESS_BUTTON));
    strings.SetString("backButton", l10n_util::GetStringUTF16(IDS_BACK_BUTTON));
  }

  strings.SetString(
      "localApprovalsButton",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_ASK_IN_PERSON_BUTTON));
  strings.SetString(
      "showDetailsLink",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_SHOW_DETAILS));
  strings.SetString(
      "hideDetailsLink",
      l10n_util::GetStringUTF16(IDS_BLOCK_INTERSTITIAL_HIDE_DETAILS));
  std::u16string request_sent_message;
  std::u16string request_failed_message;
  std::u16string request_sent_description;
  if (web_filter_interstitial_refresh_enabled) {
    request_sent_message = l10n_util::GetStringUTF16(
        IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_MESSAGE);
    request_sent_description = l10n_util::GetStringUTF16(
        second_custodian.empty()
            ? IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_SINGLE_PARENT
            : IDS_CHILD_BLOCK_INTERSTITIAL_WAITING_APPROVAL_DESCRIPTION_MULTI_PARENT);
    request_failed_message = l10n_util::GetStringUTF16(
        second_custodian.empty()
            ? IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT
            : IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT);
  } else if (second_custodian.empty()) {
    request_sent_message = l10n_util::GetStringUTF16(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_SINGLE_PARENT);
    request_failed_message = l10n_util::GetStringUTF16(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_SINGLE_PARENT);
  } else {
    request_sent_message = l10n_util::GetStringUTF16(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_SENT_MESSAGE_MULTI_PARENT);
    request_failed_message = l10n_util::GetStringUTF16(
        IDS_CHILD_BLOCK_INTERSTITIAL_REQUEST_FAILED_MESSAGE_MULTI_PARENT);
  }
  strings.SetString("requestSentMessage", request_sent_message);
  strings.SetString("requestSentDescription", request_sent_description);
  strings.SetString("requestFailedMessage", request_failed_message);
  webui::SetLoadTimeDataDefaults(app_locale, &strings);
  std::string html =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_SUPERVISED_USER_BLOCK_INTERSTITIAL_HTML);
  webui::AppendWebUiCssTextDefaults(&html);
  std::string error_html = webui::GetI18nTemplateHtml(html, &strings);
  return error_html;
}

}  //  namespace supervised_user_error_page
