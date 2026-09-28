// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/network_request_content_analysis_info.h"

#include <utility>

#include "base/check_deref.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/referrer_cache_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/enterprise/connectors/core/common.h"
#include "content/public/browser/web_contents.h"

namespace enterprise_connectors {

namespace {

Profile* GetProfile(content::WebContents& web_contents) {
  return Profile::FromBrowserContext(web_contents.GetBrowserContext());
}

}  // namespace

NetworkRequestContentAnalysisInfo::NetworkRequestContentAnalysisInfo(
    AnalysisSettings settings,
    content::WebContents* web_contents,
    GURL request_url,
    std::optional<content::GlobalRenderFrameHostId> initiator_frame_id)
    : settings_(std::move(settings)),
      request_url_(std::move(request_url)),
      user_action_id_(base::HexEncode(base::RandBytesAsVector(128))),
      // `CollectFrameUrls` supports having a null `web_contents` passed as an
      // argument so it doesn't need to be in the null check in the constructor
      // below.
      frame_url_chain_(CollectFrameUrls(web_contents,
                                        DeepScanAccessPoint::NETWORK_REQUEST,
                                        std::move(initiator_frame_id))) {
  if (web_contents) {
    tab_url_ = web_contents->GetLastCommittedURL();
    tab_title_ = base::UTF16ToUTF8(web_contents->GetTitle());
    email_ = GetProfileEmail(GetProfile(*web_contents));
    identity_manager_ =
        IdentityManagerFactory::GetForProfile(GetProfile(*web_contents));
    referrer_chain_ = GetReferrerChain(tab_url_, *web_contents);
    web_contents_ = web_contents->GetWeakPtr();
  }
}

NetworkRequestContentAnalysisInfo::~NetworkRequestContentAnalysisInfo() =
    default;

const AnalysisSettings& NetworkRequestContentAnalysisInfo::settings() const {
  return settings_;
}

signin::IdentityManager* NetworkRequestContentAnalysisInfo::identity_manager()
    const {
  return identity_manager_;
}

int NetworkRequestContentAnalysisInfo::user_action_requests_count() const {
  // Each network request is its own user action.
  return 1;
}

std::string NetworkRequestContentAnalysisInfo::tab_title() const {
  return tab_title_;
}

std::string NetworkRequestContentAnalysisInfo::user_action_id() const {
  return user_action_id_;
}

std::string NetworkRequestContentAnalysisInfo::email() const {
  return email_;
}

const GURL& NetworkRequestContentAnalysisInfo::url() const {
  return request_url_;
}

const GURL& NetworkRequestContentAnalysisInfo::tab_url() const {
  return tab_url_;
}

ContentAnalysisRequest::Reason NetworkRequestContentAnalysisInfo::reason()
    const {
  return ContentAnalysisRequest::UNKNOWN;
}

google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
NetworkRequestContentAnalysisInfo::referrer_chain() const {
  return referrer_chain_;
}

google::protobuf::RepeatedPtrField<std::string>
NetworkRequestContentAnalysisInfo::frame_url_chain() const {
  return frame_url_chain_;
}

content::WebContents* NetworkRequestContentAnalysisInfo::web_contents() const {
  return web_contents_.get();
}

}  // namespace enterprise_connectors
