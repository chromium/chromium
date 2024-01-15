// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_REQUEST_HANDLER_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_REQUEST_HANDLER_BASE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/common/proto/connectors.pb.h"

namespace enterprise_connectors {

// Abstract base class for handling the scanning and reporting of deep scanning
// requests.
// Scanning should be started using `UploadData()`. `ReportWarningBypass()` is
// only allowed to be called once scanning is complete.
// The typical flow is:
// 1. Create instance of a child class of RequestHandlerBase.
// 2. Call UploadData().
// 3. Wait for the upload to be completed.
// 4. Potentially call ReportWarningBypass() if a bypass of a warning should be
// reported.
class RequestHandlerBase {
 public:
  RequestHandlerBase(
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& user_action_id,
      const std::string& tab_title,
      uint64_t user_action_requests_count,
      safe_browsing::DeepScanAccessPoint access_point,
      ContentAnalysisRequest::Reason reason);

  virtual ~RequestHandlerBase();

  // Uploads data for deep scanning. Returns true if uploading is occurring in
  // the background and false if there is nothing to do.
  bool UploadData();

  // Moves the tokens-actions mapping of all file requests being handled to the
  // given map.
  void AppendFinalActionsTo(
      std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>*
          final_actions);

  // This method is called after a user has bypassed a scanning warning and is
  // expected to send one or more reports corresponding to the data that was
  // allowed to be transferred by the user.
  virtual void ReportWarningBypass(
      std::optional<std::u16string> user_justification) = 0;

  // After all file requests have been processed, this call can be used to
  // retrieve any final actions stored internally.  There should one for
  // each successful request.
  const std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>&
  request_tokens_to_ack_final_actions() const {
    return request_tokens_to_ack_final_actions_;
  }

 private:
  // Uploads the actual requests. To be implemented by child classes.
  // Returns true if uploading is occurring in the background and false if there
  // is nothing to do. This function is called by UploadData().
  virtual bool UploadDataImpl() = 0;

 protected:
  // Adds required fields to `request` before sending it to the binary upload
  // service.
  void PrepareRequest(enterprise_connectors::AnalysisConnector connector,
                      safe_browsing::BinaryUploadService::Request* request);

  // Returns the BinaryUploadService used to upload content for deep scanning.
  safe_browsing::BinaryUploadService* GetBinaryUploadService();

  base::WeakPtr<safe_browsing::BinaryUploadService> upload_service_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  const raw_ref<const enterprise_connectors::AnalysisSettings>
      analysis_settings_;
  GURL url_;
  std::string source_;
  std::string destination_;
  std::string user_action_id_;
  std::string tab_title_;
  uint64_t user_action_requests_count_;
  safe_browsing::DeepScanAccessPoint access_point_;
  ContentAnalysisRequest::Reason reason_;

  // A mapping of request tokens (corresponding to one user action) to their Ack
  // final action.
  std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>
      request_tokens_to_ack_final_actions_;

  base::TimeTicks upload_start_time_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_REQUEST_HANDLER_BASE_H_
