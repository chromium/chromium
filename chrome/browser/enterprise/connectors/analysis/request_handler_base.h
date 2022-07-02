// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_REQUEST_HANDLER_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_REQUEST_HANDLER_BASE_H_

#include "base/memory/raw_ptr.h"
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
      safe_browsing::DeepScanAccessPoint access_point);

  virtual ~RequestHandlerBase();

  // Uploads data for deep scanning. Returns true if uploading is occurring in
  // the background and false if there is nothing to do.
  bool UploadData();

  // This method is called after a user has bypassed a scanning warning and is
  // expected to send one or more reports corresponding to the data that was
  // allowed to be transferred by the user.
  virtual void ReportWarningBypass(
      absl::optional<std::u16string> user_justification) = 0;

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

  base::raw_ptr<safe_browsing::BinaryUploadService> upload_service_ = nullptr;
  base::raw_ptr<Profile> profile_ = nullptr;
  const enterprise_connectors::AnalysisSettings& analysis_settings_;
  GURL url_;
  safe_browsing::DeepScanAccessPoint access_point_;

  base::TimeTicks upload_start_time_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_REQUEST_HANDLER_BASE_H_
