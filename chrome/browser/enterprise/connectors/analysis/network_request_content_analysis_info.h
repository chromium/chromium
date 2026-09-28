// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_NETWORK_REQUEST_CONTENT_ANALYSIS_INFO_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_NETWORK_REQUEST_CONTENT_ANALYSIS_INFO_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise_connectors {

// `ContentAnalysisInfo` implementation for a single network request scanned
// because of the "OnNetworkRequestEnterpriseConnector" policy.
//
// Every value is computed when this class is constructed. Scanning a network
// request happens in parallel to the request itself, so the tab that made the
// request can navigate away or be closed before scanning is complete. Storing
// these values ensures the scanning request and any report sent afterwards
// describe the state of the tab when the network request was made.
class NetworkRequestContentAnalysisInfo : public ContentAnalysisInfo {
 public:
  // `web_contents` is the tab that made the network request to `request_url`.
  // `initiator_frame_id` identifies the frame that made the network request if
  // it is known, and is used to compute the frame URL chain of the request.
  NetworkRequestContentAnalysisInfo(
      AnalysisSettings settings,
      content::WebContents* web_contents,
      GURL request_url,
      std::optional<content::GlobalRenderFrameHostId> initiator_frame_id);
  ~NetworkRequestContentAnalysisInfo() override;

  NetworkRequestContentAnalysisInfo(const NetworkRequestContentAnalysisInfo&) =
      delete;
  NetworkRequestContentAnalysisInfo& operator=(
      const NetworkRequestContentAnalysisInfo&) = delete;

  // ContentAnalysisInfo:
  const AnalysisSettings& settings() const override;
  signin::IdentityManager* identity_manager() const override;
  int user_action_requests_count() const override;
  std::string tab_title() const override;
  std::string user_action_id() const override;
  std::string email() const override;
  const GURL& url() const override;
  const GURL& tab_url() const override;
  ContentAnalysisRequest::Reason reason() const override;
  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
  referrer_chain() const override;
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const override;
  content::WebContents* web_contents() const override;

 private:
  const AnalysisSettings settings_;

  // The URL the network request is sent to.
  GURL request_url_;

  // The last committed URL and title of the tab when the network request was
  // made.
  GURL tab_url_;
  std::string tab_title_;

  std::string user_action_id_;
  std::string email_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
      referrer_chain_;
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain_;
  base::WeakPtr<content::WebContents> web_contents_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_NETWORK_REQUEST_CONTENT_ANALYSIS_INFO_H_
