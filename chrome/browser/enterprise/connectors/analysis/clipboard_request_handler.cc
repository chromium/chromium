// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/clipboard_request_handler.h"

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/data_controls/reporting_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

namespace enterprise_connectors {

namespace {

ClipboardRequestHandler::TestFactory* TestFactoryStorage() {
  static base::NoDestructor<ClipboardRequestHandler::TestFactory> factory;
  return factory.get();
}

}  // namespace

// static
std::unique_ptr<ClipboardRequestHandler> ClipboardRequestHandler::Create(
    ContentAnalysisInfo* content_analysis_info,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    Type type,
    safe_browsing::DeepScanAccessPoint access_point,
    ContentMetaData::CopiedTextSource clipboard_source,
    std::string content_transfer_method,
    std::string data,
    CompletionCallback callback) {
  if (!TestFactoryStorage()->is_null()) {
    return TestFactoryStorage()->Run(content_analysis_info, upload_service,
                                     profile, std::move(url), type,
                                     access_point, std::move(clipboard_source),
                                     std::move(content_transfer_method),
                                     std::move(data), std::move(callback));
  }
  return base::WrapUnique(new ClipboardRequestHandler(
      content_analysis_info, upload_service, profile, std::move(url), type,
      access_point, std::move(clipboard_source),
      std::move(content_transfer_method), std::move(data),
      std::move(callback)));
}

// static
void ClipboardRequestHandler::SetFactoryForTesting(TestFactory factory) {
  *TestFactoryStorage() = std::move(factory);
}

// static
void ClipboardRequestHandler::ResetFactoryForTesting() {
  TestFactoryStorage()->Reset();
}

ClipboardRequestHandler::~ClipboardRequestHandler() = default;

ClipboardRequestHandler::ClipboardRequestHandler(
    ContentAnalysisInfo* content_analysis_info,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    GURL url,
    Type type,
    safe_browsing::DeepScanAccessPoint access_point,
    ContentMetaData::CopiedTextSource clipboard_source,
    std::string content_transfer_method,
    std::string data,
    CompletionCallback callback)
    : RequestHandlerBase(content_analysis_info,
                         upload_service,
                         profile,
                         std::move(url),
                         access_point),
      type_(type),
      data_(std::move(data)),
      content_size_(data_.size()),
      clipboard_source_(std::move(clipboard_source)),
      content_transfer_method_(std::move(content_transfer_method)),
      callback_(std::move(callback)) {}

void ClipboardRequestHandler::ReportWarningBypass(
    std::optional<std::u16string> user_justification) {
  ReportAnalysisConnectorWarningBypass(
      profile_, /*url*/ url_, /*tab_url*/ url_,
      /*source*/
      data_controls::ReportingService::GetClipboardSourceString(
          clipboard_source_),
      /*destination*/ url_.spec(),
      type_ == Type::kText ? "Text data" : "Image data",
      /*download_digest_sha256*/ "", type_ == Type::kText ? "text/plain" : "",
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      content_transfer_method_, access_point_, content_size_,
      content_analysis_info_->referrer_chain(), response_, user_justification);
}

void ClipboardRequestHandler::UploadForDeepScanning(
    std::unique_ptr<ClipboardAnalysisRequest> request) {
  auto* upload_service = GetBinaryUploadService();
  if (upload_service) {
    upload_service->MaybeUploadForDeepScanning(std::move(request));
  }
}

bool ClipboardRequestHandler::UploadDataImpl() {
  auto request = std::make_unique<ClipboardAnalysisRequest>(
      content_analysis_info_->settings().cloud_or_local_settings,
      std::move(data_),
      base::BindOnce(&ClipboardRequestHandler::OnContentAnalysisResponse,
                     weak_ptr_factory_.GetWeakPtr()));

  content_analysis_info_->InitializeRequest(request.get());
  request->set_analysis_connector(BULK_DATA_ENTRY);
  if (type_ == Type::kText) {
    request->set_destination(url_.spec());
    std::string source_string =
        data_controls::ReportingService::GetClipboardSourceString(
            clipboard_source_);
    if (!source_string.empty()) {
      request->set_source(source_string);
    }
    if (clipboard_source_.has_context()) {
      request->set_clipboard_source_type(clipboard_source_.context());
    }
    if (clipboard_source_.has_url()) {
      request->set_clipboard_source_url(clipboard_source_.url());
    }
  }

  UploadForDeepScanning(std::move(request));
  return true;
}

void ClipboardRequestHandler::OnContentAnalysisResponse(
    safe_browsing::BinaryUploadService::Result result,
    ContentAnalysisResponse response) {
  response_ = std::move(response);
  request_tokens_to_ack_final_actions_[response_.request_token()] =
      GetAckFinalAction(response_);

  RecordDeepScanMetrics(content_analysis_info_->settings()
                            .cloud_or_local_settings.is_cloud_analysis(),
                        access_point_,
                        base::TimeTicks::Now() - upload_start_time_,
                        content_size_, result, response_);

  auto request_handler_result = CalculateRequestHandlerResult(
      content_analysis_info_->settings(), result, response_);
  DVLOG(1) << __func__
           << (type_ == Type::kText ? ": text result=" : ": image result=")
           << request_handler_result.complies;

  bool should_warn = request_handler_result.final_result ==
                     FinalContentAnalysisResult::WARNING;

  MaybeReportDeepScanningVerdict(
      profile_, /*url*/ url_, /*tab_url*/ url_,
      /*source*/
      data_controls::ReportingService::GetClipboardSourceString(
          clipboard_source_),
      /*destination*/ url_.spec(),
      type_ == Type::kText ? "Text data" : "Image data",
      /*download_digest_sha256*/ "", type_ == Type::kText ? "text/plain" : "",
      extensions::SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
      content_transfer_method_, access_point_, content_size_,
      content_analysis_info_->referrer_chain(), result, response_,
      CalculateEventResult(content_analysis_info_->settings(),
                           request_handler_result.complies, should_warn));

  std::move(callback_).Run(std::move(request_handler_result));
}

}  // namespace enterprise_connectors
