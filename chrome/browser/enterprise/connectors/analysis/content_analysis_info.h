// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/clipboard_types.h"

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

// Simple implementation of `ContentAnalysisInfo` meant to be used for
// `GetContentAreaAccountEmail` only
class ContentAreaUserProvider : public ContentAnalysisInfo {
 public:
  static std::string GetUser(Profile* profile, const GURL& tab_url);
  static std::string GetUser(const content::ClipboardEndpoint& source);

 private:
  const AnalysisSettings& settings() const override;
  signin::IdentityManager* identity_manager() const override;
  int user_action_requests_count() const override;
  std::string tab_title() const override;
  std::string user_action_id() const override;
  std::string email() const override;
  std::string url() const override;
  const GURL& tab_url() const override;
  ContentAnalysisRequest::Reason reason() const override;
  google::protobuf::RepeatedPtrField<::safe_browsing::ReferrerChainEntry>
  referrer_chain() const override;
  google::protobuf::RepeatedPtrField<std::string> frame_url_chain()
      const override;

  explicit ContentAreaUserProvider(signin::IdentityManager* im,
                                   const GURL& tab_url);

  raw_ptr<signin::IdentityManager> im_;
  raw_ref<const GURL> tab_url_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CONTENT_ANALYSIS_INFO_H_
