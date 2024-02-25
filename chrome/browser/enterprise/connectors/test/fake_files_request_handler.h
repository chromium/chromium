// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_FILES_REQUEST_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_FILES_REQUEST_HANDLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"

namespace enterprise_connectors::test {

class FakeFilesRequestHandler : public FilesRequestHandler {
 public:
  using FakeFileRequestCallback =
      base::OnceCallback<void(base::FilePath path,
                              safe_browsing::BinaryUploadService::Result result,
                              ContentAnalysisResponse response)>;

  using FakeFileUploadCallback = base::RepeatingCallback<void(
      safe_browsing::BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
      FakeFileRequestCallback callback)>;

  FakeFilesRequestHandler(FakeFileUploadCallback fake_file_upload_callback,
                          safe_browsing::BinaryUploadService* upload_service,
                          Profile* profile,
                          const AnalysisSettings& analysis_settings,
                          GURL url,
                          const std::string& source,
                          const std::string& destination,
                          const std::string& user_action_id,
                          const std::string& tab_title,
                          const std::string& content_transfer_method,
                          safe_browsing::DeepScanAccessPoint access_point,
                          ContentAnalysisRequest::Reason reason,
                          const std::vector<base::FilePath>& paths,
                          CompletionCallback callback);

  ~FakeFilesRequestHandler() override;

  static std::unique_ptr<FilesRequestHandler> Create(
      FakeFileUploadCallback fake_file_upload_callback,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      const AnalysisSettings& analysis_settings,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& user_action_id,
      const std::string& tab_title,
      const std::string& content_transfer_method,
      safe_browsing::DeepScanAccessPoint access_point,
      ContentAnalysisRequest::Reason reason,
      const std::vector<base::FilePath>& paths,
      FilesRequestHandler::CompletionCallback callback);

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

}  // namespace enterprise_connectors::test

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_TEST_FAKE_FILES_REQUEST_HANDLER_H_
