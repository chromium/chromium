// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_request_handler.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"
#include "chrome/browser/enterprise/connectors/analysis/request_handler_base.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"

namespace enterprise_connectors {

namespace {

bool ShouldNotUploadLargePage(const AnalysisSettings& settings,
                              size_t page_size) {
  return settings.cloud_or_local_settings.is_cloud_analysis() &&
         page_size > safe_browsing::BinaryUploadService::kMaxUploadSizeBytes &&
         settings.block_large_files;
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
                         safe_browsing::DeepScanAccessPoint::PRINT),
      page_region_(std::move(page_region)),
      printer_name_(printer_name),
      page_content_type_(page_content_type),
      callback_(std::move(callback)) {}

void PagePrintRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  ReportAnalysisConnectorWarningBypass(
      profile_, GURL(content_analysis_info_->url()),
      content_analysis_info_->tab_url(), /*source*/ "",
      /*destination*/ printer_name_, content_analysis_info_->tab_title(),
      /*sha256*/ std::string(),
      /*mime_type*/ std::string(),
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      /*content_tranfer_method*/ "", safe_browsing::DeepScanAccessPoint::PRINT,
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
    safe_browsing::BinaryUploadService::Result result,
    ContentAnalysisResponse response) {
  response_ = std::move(response);
  request_tokens_to_ack_final_actions_[response_.request_token()] =
      GetAckFinalAction(response_);

  RecordDeepScanMetrics(content_analysis_info_->settings()
                            .cloud_or_local_settings.is_cloud_analysis(),
                        safe_browsing::DeepScanAccessPoint::PRINT,
                        base::TimeTicks::Now() - upload_start_time_,
                        page_size_bytes_, result, response_);

  auto request_handler_result = CalculateRequestHandlerResult(
      content_analysis_info_->settings(), result, response_);
  DVLOG(1) << __func__ << ": print result=" << request_handler_result.complies;

  bool should_warn = request_handler_result.final_result ==
                     FinalContentAnalysisResult::WARNING;

  MaybeReportDeepScanningVerdict(
      profile_, GURL(content_analysis_info_->url()),
      content_analysis_info_->tab_url(), /*source*/ "",
      /*destination*/ printer_name_, content_analysis_info_->tab_title(),
      /*sha256*/ std::string(),
      /*mime_type*/ std::string(),
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      /*content_tranfer_method*/ "", safe_browsing::DeepScanAccessPoint::PRINT,
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
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToDeepScanRequests(
      request->per_profile_request(), /*access_token*/ "", /*upload_info*/
      "Skipped - Large data blocked", /*upload_url*/ "",
      request->content_analysis_request());
  safe_browsing::WebUIInfoSingleton::GetInstance()->AddToDeepScanResponses(
      /*token=*/"",
      safe_browsing::BinaryUploadService::ResultToString(
          safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE),
      enterprise_connectors::ContentAnalysisResponse());

  request->FinishRequest(
      safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE,
      enterprise_connectors::ContentAnalysisResponse());
}

}  // namespace enterprise_connectors
