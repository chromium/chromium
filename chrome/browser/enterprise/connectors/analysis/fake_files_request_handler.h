// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FAKE_FILES_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FAKE_FILES_REQUEST_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"

namespace enterprise_connectors {

class FakeFilesRequestHandler : public FilesRequestHandler {
 public:
  using FakeFileUploadCallback = base::RepeatingCallback<void(
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)>;

  FakeFilesRequestHandler(
      FakeFileUploadCallback fake_file_upload_callback,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      GURL url,
      safe_browsing::DeepScanAccessPoint access_point,
      const std::vector<base::FilePath>& paths,
      CompletionCallback callback);

  ~FakeFilesRequestHandler() override;

  static std::unique_ptr<enterprise_connectors::FilesRequestHandler> Create(
      FakeFileUploadCallback fake_file_upload_callback,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& analysis_settings,
      GURL url,
      safe_browsing::DeepScanAccessPoint access_point,
      const std::vector<base::FilePath>& paths,
      enterprise_connectors::FilesRequestHandler::CompletionCallback callback);

  base::WeakPtr<FakeFilesRequestHandler> GetWeakPtr();

 private:
  void UploadFileForDeepScanning(
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)
      override;

  FakeFileUploadCallback fake_file_upload_callback_;
  base::WeakPtrFactory<FakeFilesRequestHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_FAKE_FILES_REQUEST_HANDLER_H_
