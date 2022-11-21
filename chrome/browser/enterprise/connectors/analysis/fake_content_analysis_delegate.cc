// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_files_request_handler.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

namespace {

base::TimeDelta response_delay = base::Seconds(0);

}  // namespace

safe_browsing::BinaryUploadService::Result
    FakeContentAnalysisDelegate::result_ =
        safe_browsing::BinaryUploadService::Result::SUCCESS;
bool FakeContentAnalysisDelegate::dialog_shown_ = false;
bool FakeContentAnalysisDelegate::dialog_canceled_ = false;

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
  if (!delete_closure_.is_null())
    delete_closure_.Run();
}

// static
void FakeContentAnalysisDelegate::SetResponseResult(
    safe_browsing::BinaryUploadService::Result result) {
  result_ = result;
}

// static
void FakeContentAnalysisDelegate::ResetDialogFlags() {
  dialog_shown_ = false;
  dialog_canceled_ = false;
}

// static
bool FakeContentAnalysisDelegate::WasDialogShown() {
  return dialog_shown_;
}

// static
bool FakeContentAnalysisDelegate::WasDialogCanceled() {
  return dialog_canceled_;
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
enterprise_connectors::ContentAnalysisResponse
FakeContentAnalysisDelegate::SuccessfulResponse(
    const std::set<std::string>& tags) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  for (const std::string& tag : tags)
    result->set_tag(tag);

  return response;
}

// static
enterprise_connectors::ContentAnalysisResponse
FakeContentAnalysisDelegate::MalwareResponse(
    enterprise_connectors::TriggeredRule::Action action) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
  result->set_tag("malware");

  auto* rule = result->add_triggered_rules();
  rule->set_action(action);

  return response;
}

// static
enterprise_connectors::ContentAnalysisResponse
FakeContentAnalysisDelegate::DlpResponse(
    enterprise_connectors::ContentAnalysisResponse::Result::Status status,
    const std::string& rule_name,
    enterprise_connectors::TriggeredRule::Action action) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* result = response.mutable_results()->Add();
  result->set_status(status);
  result->set_tag("dlp");

  auto* rule = result->add_triggered_rules();
  rule->set_rule_name(rule_name);
  rule->set_action(action);

  return response;
}

// static
enterprise_connectors::ContentAnalysisResponse
FakeContentAnalysisDelegate::MalwareAndDlpResponse(
    enterprise_connectors::TriggeredRule::Action malware_action,
    enterprise_connectors::ContentAnalysisResponse::Result::Status dlp_status,
    const std::string& dlp_rule_name,
    enterprise_connectors::TriggeredRule::Action dlp_action) {
  enterprise_connectors::ContentAnalysisResponse response;

  auto* malware_result = response.add_results();
  malware_result->set_status(
      enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
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
    absl::optional<FakeFilesRequestHandler::FakeFileRequestCallback>
        file_request_callback) {
  auto response =
      (status_callback_.is_null() ||
       result_ != safe_browsing::BinaryUploadService::Result::SUCCESS)
          ? enterprise_connectors::ContentAnalysisResponse()
          : status_callback_.Run(contents, path);
  if (request->IsAuthRequest()) {
    StringRequestCallback(result_, response);
    return;
  }

  switch (request->analysis_connector()) {
    case AnalysisConnector::BULK_DATA_ENTRY:
      StringRequestCallback(result_, response);
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
      NOTREACHED();
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

  // Simulate a response.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeContentAnalysisDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), data.contents,
                     base::FilePath(), std::move(request), absl::nullopt),
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

  // Simulate a response.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeContentAnalysisDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), std::string(), path,
                     std::move(request), std::move(callback)),
      response_delay);
}

void FakeContentAnalysisDelegate::UploadPageForDeepScanning(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  if (GetDataForTesting()
          .settings.cloud_or_local_settings.is_cloud_analysis()) {
    DCHECK_EQ(dm_token_, request->device_token());
  }

  // Simulate a response.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&FakeContentAnalysisDelegate::Response,
                     weakptr_factory_.GetWeakPtr(), std::string(),
                     base::FilePath(), std::move(request), absl::nullopt),
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

}  // namespace enterprise_connectors
