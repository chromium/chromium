// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/deep_scanning_browsertest_base.h"

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/clipboard_analysis_request.h"
#include "chrome/browser/enterprise/connectors/analysis/clipboard_request_handler.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace enterprise_connectors::test {

namespace {

constexpr char kDmToken[] = "dm_token";

constexpr base::TimeDelta kMinimumPendingDelay = base::Milliseconds(400);
constexpr base::TimeDelta kSuccessTimeout = base::Milliseconds(100);
constexpr base::TimeDelta kShowDialogDelay = base::Milliseconds(0);

class UnresponsiveFilesRequestHandler : public FilesRequestHandler {
 public:
  using FilesRequestHandler::FilesRequestHandler;

  static std::unique_ptr<FilesRequestHandler> Create(
      ContentAnalysisInfo* content_analysis_info,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      GURL url,
      const std::string& source,
      const std::string& destination,
      const std::string& content_transfer_method,
      DeepScanAccessPoint access_point,
      const std::vector<base::FilePath>& paths,
      FilesRequestHandler::CompletionCallback callback) {
    return base::WrapUnique(new UnresponsiveFilesRequestHandler(
        content_analysis_info, upload_service, profile, url, source,
        destination, content_transfer_method, access_point, paths,
        std::move(callback)));
  }

 private:
  void UploadFileForDeepScanning(
      enterprise_connectors::ScanRequestUploadResult result,
      const base::FilePath& path,
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)
      override {
    // Do nothing.
  }
};

class UnresponsiveClipboardRequestHandler : public ClipboardRequestHandler {
 public:
  using ClipboardRequestHandler::Create;

 protected:
  using ClipboardRequestHandler::ClipboardRequestHandler;

 private:
  void UploadForDeepScanning(
      std::unique_ptr<ClipboardAnalysisRequest> request) override {
    // Do nothing.
  }
};

class UnresponsiveContentAnalysisDelegate : public FakeContentAnalysisDelegate {
 public:
  using FakeContentAnalysisDelegate::FakeContentAnalysisDelegate;

  static std::unique_ptr<ContentAnalysisDelegate> Create(
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback) {
    FilesRequestHandler::SetFactoryForTesting(
        base::BindRepeating(&UnresponsiveFilesRequestHandler::Create));
    enterprise_connectors::ClipboardRequestHandler::SetFactoryForTesting(
        base::BindRepeating(&UnresponsiveClipboardRequestHandler::Create));
    return std::make_unique<UnresponsiveContentAnalysisDelegate>(
        delete_closure, status_callback, std::move(dm_token), web_contents,
        std::move(data), std::move(callback));
  }
};

}  // namespace

DeepScanningBrowserTestBase::DeepScanningBrowserTestBase() {
  // Change the time values of the upload UI to smaller ones to make tests
  // showing it run faster.
  ContentAnalysisDialogController::SetMinimumPendingDialogTimeForTesting(
      kMinimumPendingDelay);
  ContentAnalysisDialogController::SetSuccessDialogTimeoutForTesting(
      kSuccessTimeout);
  ContentAnalysisDialogController::SetShowDialogDelayForTesting(
      kShowDialogDelay);
}

DeepScanningBrowserTestBase::~DeepScanningBrowserTestBase() = default;

void DeepScanningBrowserTestBase::TearDownOnMainThread() {
  ContentAnalysisDelegate::ResetFactoryForTesting();
  FilesRequestHandler::ResetFactoryForTesting();

  ClearAnalysisConnector(browser()->profile()->GetPrefs(), FILE_ATTACHED);
  ClearAnalysisConnector(browser()->profile()->GetPrefs(), FILE_DOWNLOADED);
  ClearAnalysisConnector(browser()->profile()->GetPrefs(), BULK_DATA_ENTRY);
  ClearAnalysisConnector(browser()->profile()->GetPrefs(), PRINT);
  SetOnSecurityEventReporting(browser()->profile()->GetPrefs(), false);
}

void DeepScanningBrowserTestBase::SetUpDelegate() {
  SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &FakeContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindRepeating(&DeepScanningBrowserTestBase::StatusCallback,
                          base::Unretained(this)),
      kDmToken));
}

void DeepScanningBrowserTestBase::SetUpUnresponsiveDelegate() {
  SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &UnresponsiveContentAnalysisDelegate::Create, base::DoNothing(),
      base::BindRepeating(&DeepScanningBrowserTestBase::StatusCallback,
                          base::Unretained(this)),
      kDmToken));
}

void DeepScanningBrowserTestBase::SetQuitClosure(
    base::RepeatingClosure quit_closure) {
  quit_closure_ = quit_closure;
}

void DeepScanningBrowserTestBase::CallQuitClosure() {
  if (!quit_closure_.is_null()) {
    quit_closure_.Run();
  }
}

void DeepScanningBrowserTestBase::SetStatusCallbackResponse(
    ContentAnalysisResponse response) {
  connector_status_callback_response_ = response;
}

ContentAnalysisResponse DeepScanningBrowserTestBase::StatusCallback(
    const std::string& contents,
    const base::FilePath& path) {
  return connector_status_callback_response_;
}

void DeepScanningBrowserTestBase::CreateFilesForTest(
    const std::vector<std::string>& paths,
    const std::vector<std::string>& contents,
    ContentAnalysisDelegate::Data* data,
    const std::string& parent) {
  ASSERT_EQ(paths.size(), contents.size());

  if (!temp_dir_.IsValid()) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  base::FilePath parent_path = temp_dir_.GetPath();
  if (!parent.empty()) {
    parent_path = parent_path.AppendASCII(parent);
    ASSERT_TRUE(CreateDirectoryAndGetError(parent_path, nullptr));
    data->paths.emplace_back(parent_path);
  }

  for (size_t i = 0; i < paths.size(); ++i) {
    base::FilePath path = parent_path.AppendASCII(paths[i]);
    created_file_paths_.emplace_back(path);
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(base::as_byte_span(contents[i]));
    if (parent.empty()) {
      data->paths.emplace_back(path);
    }
  }
}

const std::vector<base::FilePath>&
DeepScanningBrowserTestBase::created_file_paths() const {
  return created_file_paths_;
}

}  // namespace enterprise_connectors::test
