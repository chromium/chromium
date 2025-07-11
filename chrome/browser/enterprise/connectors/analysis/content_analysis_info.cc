// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/connectors/core/content_area_user_provider.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace enterprise_connectors {

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
  return GetActiveContentAreaUser(identity_manager(), tab_url());
}

// static
std::string ContentAreaUserProvider::GetUser(Profile* profile,
                                             const GURL& tab_url) {
  return ContentAreaUserProvider(IdentityManagerFactory::GetForProfile(profile),
                                 tab_url)
      .GetContentAreaAccountEmail();
}

// static
std::string ContentAreaUserProvider::GetUser(
    const content::ClipboardEndpoint& source) {
  if (!source.data_transfer_endpoint() ||
      !source.data_transfer_endpoint()->IsUrlType() ||
      !source.data_transfer_endpoint()->GetURL() || !source.browser_context()) {
    return "";
  }

  return GetUser(Profile::FromBrowserContext(source.browser_context()),
                 *source.data_transfer_endpoint()->GetURL());
}

const GURL& ContentAreaUserProvider::tab_url() const {
  return *tab_url_;
}

signin::IdentityManager* ContentAreaUserProvider::identity_manager() const {
  return im_;
}

const enterprise_connectors::AnalysisSettings&
ContentAreaUserProvider::settings() const {
  NOTREACHED();
}

int ContentAreaUserProvider::user_action_requests_count() const {
  NOTREACHED();
}

std::string ContentAreaUserProvider::tab_title() const {
  NOTREACHED();
}

std::string ContentAreaUserProvider::user_action_id() const {
  NOTREACHED();
}

std::string ContentAreaUserProvider::email() const {
  NOTREACHED();
}

std::string ContentAreaUserProvider::url() const {
  NOTREACHED();
}

enterprise_connectors::ContentAnalysisRequest::Reason
ContentAreaUserProvider::reason() const {
  NOTREACHED();
}

google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
ContentAreaUserProvider::referrer_chain() const {
  NOTREACHED();
}

google::protobuf::RepeatedPtrField<std::string>
ContentAreaUserProvider::frame_url_chain() const {
  NOTREACHED();
}

ContentAreaUserProvider::ContentAreaUserProvider(signin::IdentityManager* im,
                                                 const GURL& tab_url)
    : im_(im), tab_url_(tab_url) {}

}  // namespace enterprise_connectors
