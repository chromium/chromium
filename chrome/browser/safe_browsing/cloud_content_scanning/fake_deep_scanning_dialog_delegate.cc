// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/fake_deep_scanning_dialog_delegate.h"

#include <base/callback.h>
#include <base/logging.h>

namespace safe_browsing {

FakeDeepScanningDialogDelegate::FakeDeepScanningDialogDelegate(
    base::RepeatingClosure delete_closure,
    StatusCallback status_callback,
    std::string dm_token,
    content::WebContents* web_contents,
    Data data,
    CompletionCallback callback)
    : DeepScanningDialogDelegate(web_contents,
                                 std::move(data),
                                 std::move(callback)),
      delete_closure_(delete_closure),
      status_callback_(status_callback),
      dm_token_(std::move(dm_token)) {}

FakeDeepScanningDialogDelegate::~FakeDeepScanningDialogDelegate() {
  if (!delete_closure_.is_null())
    delete_closure_.Run();
}

// static
std::unique_ptr<DeepScanningDialogDelegate>
FakeDeepScanningDialogDelegate::Create(base::RepeatingClosure delete_closure,
                                       StatusCallback status_callback,
                                       std::string dm_token,
                                       content::WebContents* web_contents,
                                       Data data,
                                       CompletionCallback callback) {
  auto ret = std::make_unique<FakeDeepScanningDialogDelegate>(
      delete_closure, status_callback, std::move(dm_token), web_contents,
      std::move(data), std::move(callback));
  return ret;
}

// static
DeepScanningClientResponse
FakeDeepScanningDialogDelegate::SuccessfulResponse() {
  DeepScanningClientResponse response;
  response.mutable_dlp_scan_verdict()->set_status(
      DlpDeepScanningVerdict::SUCCESS);
  response.mutable_malware_scan_verdict()->set_verdict(
      MalwareDeepScanningVerdict::CLEAN);
  return response;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::MalwareResponse(
    MalwareDeepScanningVerdict::Verdict verdict) {
  DeepScanningClientResponse response;
  response.mutable_dlp_scan_verdict()->set_status(
      DlpDeepScanningVerdict::SUCCESS);
  response.mutable_malware_scan_verdict()->set_verdict(verdict);
  return response;
}

// static
DeepScanningClientResponse FakeDeepScanningDialogDelegate::DlpResponse(
    DlpDeepScanningVerdict::Status status,
    const std::string& rule_name,
    DlpDeepScanningVerdict::TriggeredRule::Action action) {
  DeepScanningClientResponse response;
  response.mutable_dlp_scan_verdict()->set_status(status);
  response.mutable_malware_scan_verdict()->set_verdict(
      MalwareDeepScanningVerdict::CLEAN);

  if (!rule_name.empty()) {
    DlpDeepScanningVerdict::TriggeredRule* rule =
        response.mutable_dlp_scan_verdict()->add_triggered_rules();
    rule->set_rule_name(rule_name);
    rule->set_action(action);
  }

  return response;
}

void FakeDeepScanningDialogDelegate::Response(
    base::FilePath path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DeepScanningClientResponse response = status_callback_.is_null()
                                            ? DeepScanningClientResponse()
                                            : status_callback_.Run(path);

  if (path.empty()) {
    StringRequestCallback(BinaryUploadService::Result::SUCCESS, response);
  } else {
    FileRequestCallback(path, BinaryUploadService::Result::SUCCESS, response);
  }
}

void FakeDeepScanningDialogDelegate::UploadTextForDeepScanning(
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK_EQ(
      DlpDeepScanningClientRequest::WEB_CONTENT_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  DCHECK_EQ(dm_token_, request->deep_scanning_request().dm_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                                base::Unretained(this), base::FilePath(),
                                std::move(request)));
}

void FakeDeepScanningDialogDelegate::UploadFileForDeepScanning(
    const base::FilePath& path,
    std::unique_ptr<BinaryUploadService::Request> request) {
  DCHECK(!path.empty());
  DCHECK_EQ(
      DlpDeepScanningClientRequest::FILE_UPLOAD,
      request->deep_scanning_request().dlp_scan_request().content_source());
  DCHECK_EQ(dm_token_, request->deep_scanning_request().dm_token());

  // Simulate a response.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeDeepScanningDialogDelegate::Response,
                     base::Unretained(this), path, std::move(request)));
}

bool FakeDeepScanningDialogDelegate::CloseTabModalDialog() {
  return false;
}

}  // namespace safe_browsing
