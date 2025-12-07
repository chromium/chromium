// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_FILES_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_FILES_REQUEST_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"

namespace enterprise_connectors::test {

class FakeFilesRequestHandler : public FilesRequestHandler {
 public:
  using FakeFileRequestCallback =
      base::OnceCallback<void(base::FilePath path,
                              ScanRequestUploadResult result,
                              ContentAnalysisResponse response)>;

  using FakeFileUploadCallback = base::RepeatingCallback<void(
      ScanRequestUploadResult result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      FakeFileRequestCallback callback)>;

  FakeFilesRequestHandler(FakeFileUploadCallback fake_file_upload_callback,
                          ContentAnalysisInfo* content_analysis_info,
                          safe_browsing::BinaryUploadService* upload_service,
                          Profile* profile,
                          GURL url,
                          const std::string& source,
                          const std::string& destination,
                          const std::string& content_transfer_method,
                          DeepScanAccessPoint access_point,
                          const std::vector<base::FilePath>& paths,
                          CompletionCallback callback);

  ~FakeFilesRequestHandler() override;

  static std::unique_ptr<FilesRequestHandler> Create(
      FakeFileUploadCallback fake_file_upload_callback,
      ContentAnalysisInfo* content_analysis_info,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& content_transfer_method,
      DeepScanAccessPoint access_point,
      const std::vector<base::FilePath>& paths,
      FilesRequestHandler::CompletionCallback callback);

  base::WeakPtr<FakeFilesRequestHandler> GetWeakPtr();

 private:
  void UploadFileForDeepScanning(
      ScanRequestUploadResult result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)
      override;

  FakeFileUploadCallback fake_file_upload_callback_;
  base::WeakPtrFactory<FakeFilesRequestHandler> weak_ptr_factory_{this};
};

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_FILES_REQUEST_HANDLER_H_
