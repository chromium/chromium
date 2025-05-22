// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace enterprise_connectors {

namespace {

bool IncludeContentAreaAccountEmail(const GURL& url) {
  if (!base::FeatureList::IsEnabled(kEnterpriseActiveUserDetection)) {
    return false;
  }

  static constexpr auto kWorkspaceDomains =
      base::MakeFixedFlatSet<std::string_view>({
          "mail.google.com",
          "meet.google.com",
          "calendar.google.com",
          "drive.google.com",
          "docs.google.com",
          "sites.google.com",
          "keep.google.com",
          "script.google.com",
          "cloudsearch.google.com",
          "console.cloud.google.com",
          "datastudio.google.com",
      });

  for (const auto& domain : kWorkspaceDomains) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

std::optional<size_t> GetUserIndex(const GURL& url) {
  const re2::RE2 kUserPathRegex{"/u/\\d+/"};

  int account_id = 0;
  std::string account_id_str;

  if (re2::RE2::PartialMatch(url.path_piece(), kUserPathRegex,
                             &account_id_str)) {
    // Remove the leading "/u/" and the trailing "/".
    if (base::StringToInt(account_id_str.substr(3, account_id_str.size() - 4),
                          &account_id)) {
      return account_id;
    }
  }

  if (net::GetValueForKeyInQuery(url, "authuser", &account_id_str) &&
      base::StringToInt(account_id_str, &account_id)) {
    return account_id;
  }

  return std::nullopt;
}

}  // namespace

void ContentAnalysisInfo::InitializeRequest(
    safe_browsing::BinaryUploadService::Request* request,
    bool include_enterprise_only_fields) {
  if (include_enterprise_only_fields) {
    if (settings().cloud_or_local_settings.is_cloud_analysis()) {
      request->set_device_token(settings().cloud_or_local_settings.dm_token());
    }

    // Include tab page title in local content analysis requests.
    if (settings().cloud_or_local_settings.is_local_analysis()) {
      request->set_tab_title(tab_title());
    }

    if (settings().client_metadata) {
      request->set_client_metadata(*settings().client_metadata);
    }

    request->set_per_profile_request(settings().per_profile);

    if (reason() != ContentAnalysisRequest::UNKNOWN) {
      request->set_reason(reason());
    }

    if (base::FeatureList::IsEnabled(safe_browsing::kEnhancedFieldsForSecOps)) {
      request->set_referrer_chain(referrer_chain());
    }

    std::string email = GetContentAreaAccountEmail();
    if (!email.empty()) {
      request->set_content_area_account_email(email);
    }

    if (base::FeatureList::IsEnabled(kEnterpriseIframeDlpRulesSupport)) {
      request->set_frame_url_chain(frame_url_chain());
    }
  }

  request->set_user_action_requests_count(user_action_requests_count());
  request->set_user_action_id(user_action_id());
  request->set_email(email());
  request->set_url(url());
  request->set_tab_url(tab_url());

  for (const auto& tag : settings().tags) {
    request->add_tag(tag.first);
  }

  request->set_blocking(settings().block_until_verdict !=
                        BlockUntilVerdict::kNoBlock);
}

std::string ContentAnalysisInfo::GetContentAreaAccountEmail() const {
  if (!IncludeContentAreaAccountEmail(tab_url())) {
    return "";
  }

  auto* im = identity_manager();
  if (!im) {
    return "";
  }

  auto accounts = im->GetAccountsInCookieJar();

  if (accounts.GetAllAccounts().size() == 1) {
    return accounts.GetAllAccounts()[0].email;
  }

  size_t user_index = GetUserIndex(tab_url()).value_or(0);
  if (user_index >= accounts.GetAllAccounts().size()) {
    return "";
  }

  return accounts.GetAllAccounts()[user_index].email;
}

}  // namespace enterprise_connectors
