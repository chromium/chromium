// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PAGE_PRINT_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PAGE_PRINT_REQUEST_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"
#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {
class BinaryUploadService;
}  // namespace safe_browsing

namespace enterprise_connectors {

class ContentAnalysisInfo;

// Handles the management of a `PagePrintAnalysisRequest` and reporting for a
// single print user action. This class should be kept alive by for as long as
// there is a possibility that `ReportWarningBypass` is called, or if scanning
// is cancelled.
class PagePrintRequestHandler : public RequestHandlerBase {
 public:
  using CompletionCallback = base::OnceCallback<void(RequestHandlerResult)>;

  static std::unique_ptr<PagePrintRequestHandler> Create(
      ContentAnalysisInfo* content_analysis_info,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      GURL url,
      const std::string& printer_name,
      const std::string& page_content_type,
      base::ReadOnlySharedMemoryRegion page_region,
      CompletionCallback callback);

  using TestFactory = base::RepeatingCallback<decltype(Create)>;
  static void SetFactoryForTesting(TestFactory factory);
  static void ResetFactoryForTesting();

  ~PagePrintRequestHandler() override;

  // RequestHandlerBase:
  void ReportWarningBypass(
      std::optional<std::u16string> user_justification) override;

 protected:
  // Calls `BinaryUploadService` with the passed request to obtain a scanning
  // response. Virtual for tests.
  virtual void UploadForDeepScanning(
      std::unique_ptr<PagePrintAnalysisRequest> request);

  PagePrintRequestHandler(ContentAnalysisInfo* content_analysis_info,
                          safe_browsing::BinaryUploadService* upload_service,
                          Profile* profile,
                          GURL url,
                          const std::string& printer_name,
                          const std::string& page_content_type,
                          base::ReadOnlySharedMemoryRegion page_region,
                          CompletionCallback callback);

  // Called after obtaining a response from `BinaryUploadService`.
  void OnContentAnalysisResponse(ScanRequestUploadResult result,
                                 ContentAnalysisResponse response);

 private:
  // RequestHandlerBase:
  bool UploadDataImpl() override;

  // Called in the edge case where a printed page is too large and should be
  // exempt from scanning.
  void FinishLargeDataRequestEarly(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request);

  // The printed page to be scanned. This will be moved for scanning, so don't
  // assume this is populated for code that runs afterwards (response handling,
  // reporting).
  base::ReadOnlySharedMemoryRegion page_region_;

  // The total byte size of the printed contents. This is stored separately from
  // `page_region_` as it will have moved for scanning.
  size_t page_size_bytes_ = 0;

  std::string printer_name_;
  std::string page_content_type_;

  // The response obtained by `OnContentAnalysisResponse()`. This might be left
  // unchanged in error cases where a proper response couldn't be obtained.
  ContentAnalysisResponse response_;

  // Called after a response has been obtained from scanning, or if an error
  // `BinaryUploadService::Result` was received.
  CompletionCallback callback_;

  base::WeakPtrFactory<PagePrintRequestHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_PAGE_PRINT_REQUEST_HANDLER_H_
