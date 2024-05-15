// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"

#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/test/fake_files_request_handler.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"

namespace enterprise_connectors::test {

namespace {

base::TimeDelta response_delay = base::Seconds(0);

}  // namespace

safe_browsing::BinaryUploadService::Result
    FakeContentAnalysisDelegate::result_ =
        safe_browsing::BinaryUploadService::Result::SUCCESS;
bool FakeContentAnalysisDelegate::dialog_shown_ = false;
bool FakeContentAnalysisDelegate::dialog_canceled_ = false;
int64_t FakeContentAnalysisDelegate::total_analysis_requests_count_ = 0;

FakeContentAnalysisDelegate::FakeContentAnalysisDelegate(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback)
    : ContentAnalysisDelegate(web_contents,
                              std::move(data),
                              std::move(callback),
                              safe_browsing::DeepScanAccessPoint::UPLOAD),
      delete_closure_(delete_closure),
      status_callback_(status_callback),
      dm_token_(std::move(dm_token)) {}

FakeContentAnalysisDelegate::~FakeContentAnalysisDelegate() {
  if (!delete_closure_.is_null()) {
    delete_closure_.Run();
  }
}

// static
void FakeContentAnalysisDelegate::SetResponseResult(
    safe_browsing::BinaryUploadService::Result result) {
  result_ = result;
}

// static
void FakeContentAnalysisDelegate::
    ResetStaticDialogFlagsAndTotalRequestsCount() {
  dialog_shown_ = false;
  dialog_canceled_ = false;
  total_analysis_requests_count_ = 0;
}

// static
bool FakeContentAnalysisDelegate::WasDialogShown() {
  return dialog_shown_;
}

// static
bool FakeContentAnalysisDelegate::WasDialogCanceled() {
  return dialog_canceled_;
}

int FakeContentAnalysisDelegate::GetTotalAnalysisRequestsCount() {
  return total_analysis_requests_count_;
}

// static
std::unique_ptr<ContentAnalysisDelegate> FakeContentAnalysisDelegate::Create(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback) {
  auto ret = std::make_unique<FakeContentAnalysisDelegate>(
      delete_closure, status_callback, std::move(dm_token), web_contents,
      std::move(data), std::move(callback));
  FilesRequestHandler::SetFactoryForTesting(base::BindRepeating(
      &FakeFilesRequestHandler::Create,
      base::BindRepeating(
          &FakeContentAnalysisDelegate::FakeUploadFileForDeepScanning,
          base::Unretained(ret.get()))));
  return ret;
}

// static
void FakeContentAnalysisDelegate::SetResponseDelay(base::TimeDelta delay) {
  response_delay = delay;
}

// static
ContentAnalysisResponse FakeContentAnalysisDelegate::SuccessfulResponse(
    const std::set<std::string>& tags) {
  ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  for (const std::string& tag : tags) {
    result->set_tag(tag);
  }

  return response;
}

// static
ContentAnalysisResponse FakeContentAnalysisDelegate::MalwareResponse(
    TriggeredRule::Action action) {
  ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("malware");

  auto* rule = result->add_triggered_rules();
  rule->set_action(action);

  return response;
}

// static
ContentAnalysisResponse FakeContentAnalysisDelegate::DlpResponse(
    ContentAnalysisResponse::Result::Status status,
    const std::string& rule_name,
    TriggeredRule::Action action) {
  ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(status);
  result->set_tag("dlp");

  auto* rule = result->add_triggered_rules();
  rule->set_rule_name(rule_name);
  rule->set_action(action);

  return response;
}

// static
ContentAnalysisResponse FakeContentAnalysisDelegate::MalwareAndDlpResponse(
    TriggeredRule::Action malware_action,
    ContentAnalysisResponse::Result::Status dlp_status,
    const std::string& dlp_rule_name,
    TriggeredRule::Action dlp_action) {
  ContentAnalysisResponse response;

  auto* malware_result = response.add_results();
  malware_result->set_status(ContentAnalysisResponse::Result::SUCCESS);
  malware_result->set_tag("malware");
  auto* malware_rule = malware_result->add_triggered_rules();
  malware_rule->set_action(malware_action);

  auto* dlp_result = response.add_results();
  dlp_result->set_status(dlp_status);
  dlp_result->set_tag("dlp");
  auto* dlp_rule = dlp_result->add_triggered_rules();
  dlp_rule->set_rule_name(dlp_rule_name);
  dlp_rule->set_action(dlp_action);

  return response;
}

void FakeContentAnalysisDelegate::Response(
    std::string contents,
    base::FilePath path,
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
    std::optional<FakeFilesRequestHandler::FakeFileRequestCallback>
        file_request_callback,
    bool is_image_request) {
  auto response =
      (status_callback_.is_null() ||
       result_ != safe_browsing::BinaryUploadService::Result::SUCCESS)
          ? ContentAnalysisResponse()
          : status_callback_.Run(contents, path);
  if (request->IsAuthRequest()) {
    StringRequestCallback(result_, response);
    return;
  }

  switch (request->analysis_connector()) {
    case AnalysisConnector::BULK_DATA_ENTRY:
      if (is_image_request) {
        ImageRequestCallback(result_, response);
      } else {
        StringRequestCallback(result_, response);
      }
      break;
    case AnalysisConnector::FILE_ATTACHED:
    case AnalysisConnector::FILE_DOWNLOADED:
      DCHECK(file_request_callback.has_value());
      std::move(file_request_callback.value()).Run(path, result_, response);
      break;
    case AnalysisConnector::PRINT:
      PageRequestCallback(result_, response);
      break;
    case AnalysisConnector::FILE_TRANSFER:
    case AnalysisConnector::ANALYSIS_CONNECTOR_UNSPECIFIED:
      NOTREACHED_IN_MIGRATION();
  }
}

void FakeContentAnalysisDelegate::UploadTextForDeepScanning(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  if (GetDataForTesting()
          .settings.cloud_or_local_settings.is_cloud_analysis()) {
    DCHECK_EQ(dm_token_, request->device_token());
  }

  // For text requests, GetRequestData() is synchronous.
  safe_browsing::BinaryUploadService::Request::Data data;
  request->GetRequestData(base::BindLambdaForTesting(
      [&data](safe_browsing::BinaryUploadService::Result,
              safe_browsing::BinaryUploadService::Request::Data data_arg) {
        data = std::move(data_arg);
      }));

  // Increment total analysis request count.
  total_analysis_requests_count_++;

  // Simulate a response.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeContentAnalysisDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), data.contents,
                     base::FilePath(), std::move(request), std::nullopt, false),
      response_delay);
}

void FakeContentAnalysisDelegate::UploadImageForDeepScanning(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  if (GetDataForTesting()
          .settings.cloud_or_local_settings.is_cloud_analysis()) {
    DCHECK_EQ(dm_token_, request->device_token());
  }

  // For image requests, GetRequestData() is synchronous.
  safe_browsing::BinaryUploadService::Request::Data data;
  request->GetRequestData(base::BindLambdaForTesting(
      [&data](safe_browsing::BinaryUploadService::Result,
              safe_browsing::BinaryUploadService::Request::Data data_arg) {
        data = std::move(data_arg);
      }));

  // Increment total analysis request count.
  total_analysis_requests_count_++;

  // Simulate a response.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeContentAnalysisDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), data.contents,
                     base::FilePath(), std::move(request), std::nullopt, true),
      response_delay);
}

void FakeContentAnalysisDelegate::FakeUploadFileForDeepScanning(
    safe_browsing::BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
    FakeFilesRequestHandler::FakeFileRequestCallback callback) {
  DCHECK(!path.empty());
  if (GetDataForTesting()
          .settings.cloud_or_local_settings.is_cloud_analysis()) {
    DCHECK_EQ(dm_token_, request->device_token());
  }

  // Increment total analysis request count.
  total_analysis_requests_count_++;

  // Simulate a response.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeContentAnalysisDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), std::string(), path,
                     std::move(request), std::move(callback), false),
      response_delay);
}

void FakeContentAnalysisDelegate::UploadPageForDeepScanning(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  if (GetDataForTesting()
          .settings.cloud_or_local_settings.is_cloud_analysis()) {
    DCHECK_EQ(dm_token_, request->device_token());
  }

  // Increment total analysis request count.
  total_analysis_requests_count_++;

  // Simulate a response.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeContentAnalysisDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), std::string(),
                     base::FilePath(), std::move(request), std::nullopt, false),
      response_delay);
}

bool FakeContentAnalysisDelegate::ShowFinalResultInDialog() {
  dialog_shown_ = true;
  return ContentAnalysisDelegate::ShowFinalResultInDialog();
}

bool FakeContentAnalysisDelegate::CancelDialog() {
  dialog_canceled_ = true;
  return ContentAnalysisDelegate::CancelDialog();
}

safe_browsing::BinaryUploadService*
FakeContentAnalysisDelegate::GetBinaryUploadService() {
  // This class overrides the upload service, so just return null here.
  return nullptr;
}

}  // namespace enterprise_connectors::test
