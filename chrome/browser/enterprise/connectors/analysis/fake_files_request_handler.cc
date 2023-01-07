// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/fake_files_request_handler.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"

namespace enterprise_connectors {

FakeFilesRequestHandler::FakeFilesRequestHandler(
    FakeFileUploadCallback fake_file_upload_callback,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    GURL url,
    const std::string& source,
    const std::string& destination,
    const std::string& user_action_id,
    safe_browsing::DeepScanAccessPoint access_point,
    const std::vector<base::FilePath>& paths,
    CompletionCallback callback)
    : enterprise_connectors::FilesRequestHandler(upload_service,
                                                 profile,
                                                 analysis_settings,
                                                 url,
                                                 source,
                                                 destination,
                                                 user_action_id,
                                                 access_point,
                                                 paths,
                                                 std::move(callback)),
      fake_file_upload_callback_(fake_file_upload_callback) {}

FakeFilesRequestHandler::~FakeFilesRequestHandler() = default;

// static
std::unique_ptr<enterprise_connectors::FilesRequestHandler>
FakeFilesRequestHandler::Create(
    FakeFileUploadCallback fake_file_upload_callback,
    safe_browsing::BinaryUploadService* upload_service,
    Profile* profile,
    const enterprise_connectors::AnalysisSettings& analysis_settings,
    GURL url,
    const std::string& source,
    const std::string& destination,
    const std::string& user_action_id,
    safe_browsing::DeepScanAccessPoint access_point,
    const std::vector<base::FilePath>& paths,
    enterprise_connectors::FilesRequestHandler::CompletionCallback callback) {
  return std::make_unique<FakeFilesRequestHandler>(
      fake_file_upload_callback, upload_service, profile, analysis_settings,
      url, source, destination, user_action_id, access_point, paths,
      std::move(callback));
}

void FakeFilesRequestHandler::UploadFileForDeepScanning(
    safe_browsing::BinaryUploadService::Result result,
    const base::FilePath& path,
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request) {
  fake_file_upload_callback_.Run(
      result, path, std::move(request),
      base::BindOnce(&FakeFilesRequestHandler::FileRequestCallbackForTesting,
                     GetWeakPtr()));
}

base::WeakPtr<FakeFilesRequestHandler> FakeFilesRequestHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace enterprise_connectors
