// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/download/public/common/download_item.h"
#include "components/enterprise/connectors/core/content_area_user_provider.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
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
  if (!CanRetrieveActiveUser(tab_url())) {
    return "";
  }

  std::string email = GetActiveContentAreaUser(identity_manager(), tab_url());
  if (!email.empty()) {
    return email;
  }

  if (web_contents()) {
    web_contents()->GetOutermostWebContents()->ForEachRenderFrameHost(
        [&email, this](content::RenderFrameHost* rfh) {
          if (email.empty()) {
            email = GetActiveFrameUser(identity_manager(), tab_url(),
                                       rfh->GetLastCommittedURL());
          }
        });
  }

  if (!email.empty()) {
    return email;
  }

  auto referrers = referrer_chain();
  for (const auto& referrer : referrers) {
    GURL referrer_url(referrer.url());
    if (referrer_url.is_valid()) {
      email = GetActiveContentAreaUser(identity_manager(), referrer_url);

      if (!email.empty()) {
        break;
      }
    }
  }
  return email;
}

// static
std::string ContentAreaUserProvider::GetUser(Profile* profile,
                                             content::WebContents* web_contents,
                                             const GURL& tab_url) {
  return ContentAreaUserProvider(
             IdentityManagerFactory::GetForProfile(profile),
             web_contents, tab_url)
      .GetContentAreaAccountEmail();
}

// static
std::string ContentAreaUserProvider::GetUser(
    const content::ClipboardEndpoint& endpoint) {
  if (!endpoint.data_transfer_endpoint() ||
      !endpoint.data_transfer_endpoint()->IsUrlType() ||
      !endpoint.data_transfer_endpoint()->GetURL() ||
      !endpoint.browser_context()) {
    return "";
  }

  return GetUser(Profile::FromBrowserContext(endpoint.browser_context()),
                 endpoint.web_contents(),
                 *endpoint.data_transfer_endpoint()->GetURL());
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

const GURL& ContentAreaUserProvider::url() const {
  NOTREACHED();
}

enterprise_connectors::ContentAnalysisRequest::Reason
ContentAreaUserProvider::reason() const {
  NOTREACHED();
}

google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
ContentAreaUserProvider::referrer_chain() const {
  return referrer_chain_;
}

google::protobuf::RepeatedPtrField<std::string>
ContentAreaUserProvider::frame_url_chain() const {
  NOTREACHED();
}

content::WebContents* ContentAreaUserProvider::web_contents() const {
  return web_contents_.get();
}

ContentAreaUserProvider::ContentAreaUserProvider(
    signin::IdentityManager* im,
    content::WebContents* web_contents,
    const GURL& tab_url)
    : im_(im),
      web_contents_(web_contents ? web_contents->GetWeakPtr() : nullptr),
      tab_url_(tab_url) {
  if (web_contents) {
    referrer_chain_ =
        enterprise_connectors::GetReferrerChain(tab_url, *web_contents);
  }
}

ContentAreaUserProvider::~ContentAreaUserProvider() = default;

DownloadContentAreaUserProvider::DownloadContentAreaUserProvider(
    download::DownloadItem& download_item)
    : url_(download_item.GetURL()),
      tab_url_(download_item.GetTabUrl()),
      im_(IdentityManagerFactory::GetForProfile(Profile::FromBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(&download_item)))),
      web_contents_(
          content::DownloadItemUtils::GetOriginalWebContents(&download_item)
              ? content::DownloadItemUtils::GetOriginalWebContents(
                    &download_item)
                    ->GetWeakPtr()
              : nullptr) {
  referrer_chain_ =
      safe_browsing::GetOrIdentifyReferrerChainForEnterprise(download_item);
}

DownloadContentAreaUserProvider::~DownloadContentAreaUserProvider() = default;

const GURL& DownloadContentAreaUserProvider::url() const {
  return url_;
}

const GURL& DownloadContentAreaUserProvider::tab_url() const {
  return tab_url_;
}

signin::IdentityManager* DownloadContentAreaUserProvider::identity_manager()
    const {
  return im_;
}

content::WebContents* DownloadContentAreaUserProvider::web_contents() const {
  return web_contents_.get();
}

const enterprise_connectors::AnalysisSettings&
DownloadContentAreaUserProvider::settings() const {
  NOTREACHED();
}

int DownloadContentAreaUserProvider::user_action_requests_count() const {
  NOTREACHED();
}

std::string DownloadContentAreaUserProvider::tab_title() const {
  NOTREACHED();
}

std::string DownloadContentAreaUserProvider::user_action_id() const {
  NOTREACHED();
}

std::string DownloadContentAreaUserProvider::email() const {
  NOTREACHED();
}

enterprise_connectors::ContentAnalysisRequest::Reason
DownloadContentAreaUserProvider::reason() const {
  NOTREACHED();
}

google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
DownloadContentAreaUserProvider::referrer_chain() const {
  return referrer_chain_;
}

google::protobuf::RepeatedPtrField<std::string>
DownloadContentAreaUserProvider::frame_url_chain() const {
  return frame_url_chain_;
}

}  // namespace enterprise_connectors
