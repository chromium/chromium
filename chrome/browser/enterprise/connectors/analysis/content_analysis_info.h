// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise_connectors {

// Interface providing data about a given content analysis action. This should
// be used as an abstraction layer to access information about some content
// analysis context when the exact action that triggered is not important (ex.
// when populating protos).
class ContentAnalysisInfo {
 public:
  // The `AnalysisSettings` that should be applied to the content analysis scan.
  virtual const AnalysisSettings& settings() const = 0;

  // The `signin::IdentityManager` that corresponds to the browser context where
  // content analysis is taking place.
  virtual signin::IdentityManager* identity_manager() const = 0;

  // These methods correspond to fields in `BinaryUploadService::Request`.
  virtual int user_action_requests_count() const = 0;
  virtual std::string tab_title() const = 0;
  virtual std::string user_action_id() const = 0;
  virtual std::string email() const = 0;
  virtual std::string url() const = 0;
  virtual const GURL& tab_url() const = 0;
  virtual ContentAnalysisRequest::Reason reason() const = 0;
  virtual google::protobuf::RepeatedPtrField<
      ::safe_browsing::ReferrerChainEntry>
  referrer_chain() const = 0;
  virtual google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const = 0;

  // Adds shared fields to `request` before sending it to the binary upload
  // service. Connector-specific fields need to be added to the request
  // separately.
  void InitializeRequest(safe_browsing::BinaryUploadService::Request* request,
                         bool include_enterprise_only_fields = true);

  // Returns email of the active Gaia user based on the values provided by
  // `tab_url()` and `identity_manager()`. Only returns a value for Workspace
  // sites.
  // TODO(crbug.com/415002299): Add tests for this.
  std::string GetContentAreaAccountEmail() const;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
