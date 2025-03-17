// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CLIPBOARD_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CLIPBOARD_REQUEST_HANDLER_H_

#include "chrome/browser/enterprise/connectors/analysis/clipboard_analysis_request.h"
#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

class Profile;

namespace safe_browsing {
class BinaryUploadService;
}  // namespace safe_browsing

namespace enterprise_connectors {

class ContentAnalysisInfo;

// Handles the management of a `ClipboardAnalysisRequest` and reporting for a
// single paste user action. This class should be kept alive by for as long as
// there is a possibility that `ReportWarningBypass` is called, or if scanning
// is cancelled.
class ClipboardRequestHandler : public RequestHandlerBase {
 public:
  using CompletionCallback = base::OnceCallback<void(RequestHandlerResult)>;

  enum class Type { kText, kImage };

  static std::unique_ptr<ClipboardRequestHandler> Create(
      ContentAnalysisInfo* content_analysis_info,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      GURL url,
      Type type,
      safe_browsing::DeepScanAccessPoint access_point,
      ContentMetaData::CopiedTextSource clipboard_source,
      std::string content_transfer_method,
      std::string data,
      CompletionCallback callback);

  using TestFactory = base::RepeatingCallback<decltype(Create)>;
  static void SetFactoryForTesting(TestFactory factory);
  static void ResetFactoryForTesting();

  ~ClipboardRequestHandler() override;

  // Called after obtaining a response from `BinaryUploadService`.
  void OnContentAnalysisResponse(
      safe_browsing::BinaryUploadService::Result result,
      ContentAnalysisResponse response);

  // RequestHandlerBase:
  void ReportWarningBypass(
      std::optional<std::u16string> user_justification) override;

 protected:
  // Calls `BinaryUploadService` with the passed request to obtain a scanning
  // response. Virtual for tests.
  virtual void UploadForDeepScanning(
      std::unique_ptr<ClipboardAnalysisRequest> request);

  ClipboardRequestHandler(ContentAnalysisInfo* content_analysis_info,
                          safe_browsing::BinaryUploadService* upload_service,
                          Profile* profile,
                          GURL url,
                          Type type,
                          safe_browsing::DeepScanAccessPoint access_point,
                          ContentMetaData::CopiedTextSource clipboard_source,
                          std::string content_transfer_method,
                          std::string data,
                          CompletionCallback callback);

  // Protected instead of private for test sub-classes.
  Type type_;

 private:
  // RequestHandlerBase:
  bool UploadDataImpl() override;

  // The bytes of the text or image to scan. This will be moved for scanning, so
  // don't assumed this is still valid data for code that runs afterwards
  // (response handling, reporting).
  std::string data_;

  // The total bytes of the pasted text or image. This is stored separately from
  // `data_` as it will have moved for scanning.
  size_t content_size_ = 0;

  ContentMetaData::CopiedTextSource clipboard_source_;
  std::string content_transfer_method_;

  // The response obtained by `OnContentAnalysisResponse()`. This might be left
  // unchanged in error cases where a proper response couldn't be obtained.
  ContentAnalysisResponse response_;

  // Called after a response has been obtained from scanning, or if an error
  // `BinaryUploadService::Result` was received.
  CompletionCallback callback_;

  base::WeakPtrFactory<ClipboardRequestHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_CLIPBOARD_REQUEST_HANDLER_H_
