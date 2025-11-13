// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_request_handler.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"
#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/safe_browsing/content/browser/web_ui/web_ui_content_info_singleton.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kMaxPagePrintUploadSizeMetricsKB = 500 * 1024;

bool ShouldNotUploadLargePage(const AnalysisSettings& settings,
                              size_t page_size) {
  size_t max_page_size_bytes =
      safe_browsing::BinaryUploadService::kMaxUploadSizeBytes;
  if (base::FeatureList::IsEnabled(
          enterprise_connectors::kEnableNewUploadSizeLimit)) {
    max_page_size_bytes =
        1024 * 1024 *
        enterprise_connectors::kMaxContentAnalysisFileSizeMB.Get();
  }

  return settings.cloud_or_local_settings.is_cloud_analysis() &&
         page_size > max_page_size_bytes && settings.block_large_files;
}

PagePrintRequestHandler::TestFactory* TestFactoryStorage() {
  static base::NoDestructor<PagePrintRequestHandler::TestFactory> factory;
  return factory.get();
}

}  // namespace

// static
std::unique_ptr<PagePrintRequestHandler> PagePrintRequestHandler::Create(
    ContentAnalysisInfo* content_analysis_info,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    const std::string& printer_name,
    const std::string& page_content_type,
    base::ReadOnlySharedMemoryRegion page_region,
    CompletionCallback callback) {
  if (!TestFactoryStorage()->is_null()) {
    return TestFactoryStorage()->Run(content_analysis_info, upload_service,
                                     profile, std::move(url), printer_name,
                                     page_content_type, std::move(page_region),
                                     std::move(callback));
  }
  return base::WrapUnique(new PagePrintRequestHandler(
      content_analysis_info, upload_service, profile, std::move(url),
      printer_name, page_content_type, std::move(page_region),
      std::move(callback)));
}

// static
void PagePrintRequestHandler::SetFactoryForTesting(TestFactory factory) {
  *TestFactoryStorage() = std::move(factory);
}

// static
void PagePrintRequestHandler::ResetFactoryForTesting() {
  TestFactoryStorage()->Reset();
}

PagePrintRequestHandler::~PagePrintRequestHandler() = default;

PagePrintRequestHandler::PagePrintRequestHandler(
    ContentAnalysisInfo* content_analysis_info,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    const std::string& printer_name,
    const std::string& page_content_type,
    base::ReadOnlySharedMemoryRegion page_region,
    CompletionCallback callback)
    : RequestHandlerBase(content_analysis_info,
                         upload_service,
                         profile,
                         url,
                         DeepScanAccessPoint::PRINT),
      page_region_(std::move(page_region)),
      printer_name_(printer_name),
      page_content_type_(page_content_type),
      callback_(std::move(callback)) {}

void PagePrintRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  ReportAnalysisConnectorWarningBypass(
      profile_, *content_analysis_info_, /*source*/ "",
      /*destination*/ printer_name_, content_analysis_info_->tab_title(),
      /*sha256*/ std::string(),
      /*mime_type*/ std::string(), kPagePrintDataTransferEventTrigger,
      /*content_tranfer_method*/ "",
      /*content_size*/ -1, content_analysis_info_->referrer_chain(), response_,
      user_justification);
}

void PagePrintRequestHandler::UploadForDeepScanning(
    std::unique_ptr<PagePrintAnalysisRequest> request) {
  auto* upload_service = GetBinaryUploadService();
  if (upload_service) {
    upload_service->MaybeUploadForDeepScanning(std::move(request));
  }
}

bool PagePrintRequestHandler::UploadDataImpl() {
  page_size_bytes_ = page_region_.GetSize();
  auto request = std::make_unique<PagePrintAnalysisRequest>(
      content_analysis_info_->settings(), std::move(page_region_),
      base::BindOnce(&PagePrintRequestHandler::OnContentAnalysisResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  content_analysis_info_->InitializeRequest(request.get());
  request->set_analysis_connector(PRINT);
  request->set_filename(content_analysis_info_->tab_title());
  if (!printer_name_.empty()) {
    request->set_printer_name(printer_name_);
  }
  if (!page_content_type_.empty()) {
    request->set_content_type(page_content_type_);
  }

  // Create a histogram to track the size of printed pages being scanned up to
  // 500MB.
  base::UmaHistogramCustomCounts(
      "Enterprise.FileAnalysisRequest.PrintedPageSize", page_size_bytes_ / 1024,
      1, kMaxPagePrintUploadSizeMetricsKB, 50);

  if (ShouldNotUploadLargePage(content_analysis_info_->settings(),
                               page_size_bytes_)) {
    // The request shouldn't be finished early synchronously so that
    // `UploadData()` can return "false" to `CreateForWebContents()` and let
    // it initialize the tab modal dialog.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PagePrintRequestHandler::FinishLargeDataRequestEarly,
                       weak_ptr_factory_.GetWeakPtr(), std::move(request)));
  } else {
    UploadForDeepScanning(std::move(request));
  }

  return true;
}

void PagePrintRequestHandler::OnContentAnalysisResponse(
    ScanRequestUploadResult result,
    ContentAnalysisResponse response) {
  response_ = std::move(response);
  request_tokens_to_ack_final_actions_[response_.request_token()] =
      GetAckFinalAction(response_);

  safe_browsing::RecordDeepScanMetrics(
      content_analysis_info_->settings()
          .cloud_or_local_settings.is_cloud_analysis(),
      DeepScanAccessPoint::PRINT, base::TimeTicks::Now() - upload_start_time_,
      page_size_bytes_, result, response_);

  auto request_handler_result = CalculateRequestHandlerResult(
      content_analysis_info_->settings(), result, response_);
  DVLOG(1) << __func__ << ": print result=" << request_handler_result.complies;

  bool should_warn = request_handler_result.final_result ==
                     FinalContentAnalysisResult::WARNING;

  MaybeReportDeepScanningVerdict(
      profile_, content_analysis_info_.get(),
      /*source*/ "",
      /*destination*/ printer_name_, content_analysis_info_->tab_title(),
      /*sha256*/ std::string(),
      /*mime_type*/ std::string(), kPagePrintDataTransferEventTrigger,
      /*content_tranfer_method*/ "",
      content_analysis_info_->GetContentAreaAccountEmail(),
      /*content_size*/ -1, content_analysis_info_->referrer_chain(), result,
      response_,
      CalculateEventResult(content_analysis_info_->settings(),
                           request_handler_result.complies, should_warn));

  std::move(callback_).Run(std::move(request_handler_result));
}

void PagePrintRequestHandler::FinishLargeDataRequestEarly(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  // We add the request here in case we never actually uploaded anything, so
  // it wasn't added in OnGetRequestData
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanRequests(request->per_profile_request(),
                              /*access_token*/ "", /*upload_info*/
                              "Skipped - Large data blocked", /*upload_url*/ "",
                              request->content_analysis_request());
  safe_browsing::WebUIContentInfoSingleton::GetInstance()
      ->AddToDeepScanResponses(
          /*token=*/"",
          ScanRequestUploadResultToString(
              ScanRequestUploadResult::FILE_TOO_LARGE),
          enterprise_connectors::ContentAnalysisResponse());

  request->FinishRequest(ScanRequestUploadResult::FILE_TOO_LARGE,
                         enterprise_connectors::ContentAnalysisResponse());
}

}  // namespace enterprise_connectors
