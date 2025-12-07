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
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
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
  RequestHandlerBase(ContentAnalysisInfo* content_analysis_info,
                     safe_browsing::BinaryUploadService* upload_service,
                     Profile* profile,
                     GURL url,
                     DeepScanAccessPoint access_point);

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
  // Returns the BinaryUploadService used to upload content for deep scanning.
  safe_browsing::BinaryUploadService* GetBinaryUploadService();

  raw_ptr<ContentAnalysisInfo> content_analysis_info_ = nullptr;
  base::WeakPtr<safe_browsing::BinaryUploadService> upload_service_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  GURL url_;
  DeepScanAccessPoint access_point_;

  // A mapping of request tokens (corresponding to one user action) to their Ack
  // final action.
  std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>
      request_tokens_to_ack_final_actions_;

  base::TimeTicks upload_start_time_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_REQUEST_HANDLER_BASE_H_
