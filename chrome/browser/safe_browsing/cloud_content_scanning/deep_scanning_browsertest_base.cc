// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_browsertest_base.h"
#include "base/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"

namespace safe_browsing {

namespace {

constexpr char kDmToken[] = "dm_token";

constexpr base::TimeDelta kMinimumPendingDelay =
    base::TimeDelta::FromMilliseconds(400);
constexpr base::TimeDelta kSuccessTimeout =
    base::TimeDelta::FromMilliseconds(100);

class UnresponsiveContentAnalysisDelegate
    : public enterprise_connectors::FakeContentAnalysisDelegate {
 public:
  using enterprise_connectors::FakeContentAnalysisDelegate::
      FakeContentAnalysisDelegate;

  static std::unique_ptr<enterprise_connectors::ContentAnalysisDelegate> Create(
      base::RepeatingClosure delete_closure,
      StatusCallback status_callback,
      EncryptionStatusCallback encryption_callback,
      std::string dm_token,
      content::WebContents* web_contents,
      Data data,
      CompletionCallback callback) {
    auto ret = std::make_unique<UnresponsiveContentAnalysisDelegate>(
        delete_closure, status_callback, encryption_callback,
        std::move(dm_token), web_contents, std::move(data),
        std::move(callback));
    return ret;
  }

 private:
  void UploadTextForDeepScanning(
      std::unique_ptr<BinaryUploadService::Request> request) override {
    // Do nothing.
  }

  void UploadFileForDeepScanning(
      BinaryUploadService::Result result,
      const base::FilePath& path,
      std::unique_ptr<BinaryUploadService::Request> request) override {
    // Do nothing.
  }
};

}  // namespace

DeepScanningBrowserTestBase::DeepScanningBrowserTestBase() {
  // Enable every deep scanning features.
  scoped_feature_list_.InitWithFeatures(
      {enterprise_connectors::kEnterpriseConnectorsEnabled}, {});

  // Change the time values of the upload UI to smaller ones to make tests
  // showing it run faster.
  enterprise_connectors::ContentAnalysisDialog::
      SetMinimumPendingDialogTimeForTesting(kMinimumPendingDelay);
  enterprise_connectors::ContentAnalysisDialog::
      SetSuccessDialogTimeoutForTesting(kSuccessTimeout);
}

DeepScanningBrowserTestBase::~DeepScanningBrowserTestBase() = default;

void DeepScanningBrowserTestBase::TearDownOnMainThread() {
  enterprise_connectors::ContentAnalysisDelegate::ResetFactoryForTesting();

  ClearAnalysisConnector(browser()->profile()->GetPrefs(),
                         enterprise_connectors::FILE_ATTACHED);
  ClearAnalysisConnector(browser()->profile()->GetPrefs(),
                         enterprise_connectors::FILE_DOWNLOADED);
  ClearAnalysisConnector(browser()->profile()->GetPrefs(),
                         enterprise_connectors::BULK_DATA_ENTRY);
  SetOnSecurityEventReporting(browser()->profile()->GetPrefs(), false);
}

void DeepScanningBrowserTestBase::SetUpDelegate() {
  SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating(&DeepScanningBrowserTestBase::StatusCallback,
                              base::Unretained(this)),
          base::BindRepeating(
              &DeepScanningBrowserTestBase::EncryptionStatusCallback,
              base::Unretained(this)),
          kDmToken));
}

void DeepScanningBrowserTestBase::SetUpUnresponsiveDelegate() {
  SetDMTokenForTesting(policy::DMToken::CreateValidTokenForTesting(kDmToken));
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &UnresponsiveContentAnalysisDelegate::Create, base::DoNothing(),
          base::BindRepeating(&DeepScanningBrowserTestBase::StatusCallback,
                              base::Unretained(this)),
          base::BindRepeating(
              &DeepScanningBrowserTestBase::EncryptionStatusCallback,
              base::Unretained(this)),
          kDmToken));
}

void DeepScanningBrowserTestBase::SetQuitClosure(
    base::RepeatingClosure quit_closure) {
  quit_closure_ = quit_closure;
}

void DeepScanningBrowserTestBase::CallQuitClosure() {
  if (!quit_closure_.is_null())
    quit_closure_.Run();
}

void DeepScanningBrowserTestBase::SetStatusCallbackResponse(
    enterprise_connectors::ContentAnalysisResponse response) {
  connector_status_callback_response_ = response;
}

enterprise_connectors::ContentAnalysisResponse
DeepScanningBrowserTestBase::StatusCallback(const base::FilePath& path) {
  return connector_status_callback_response_;
}

bool DeepScanningBrowserTestBase::EncryptionStatusCallback(
    const base::FilePath& path) {
  return false;
}

void DeepScanningBrowserTestBase::CreateFilesForTest(
    const std::vector<std::string>& paths,
    const std::vector<std::string>& contents,
    enterprise_connectors::ContentAnalysisDelegate::Data* data) {
  ASSERT_EQ(paths.size(), contents.size());

  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  for (size_t i = 0; i < paths.size(); ++i) {
    base::FilePath path = temp_dir_.GetPath().AppendASCII(paths[i]);
    created_file_paths_.emplace_back(path);
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    file.WriteAtCurrentPos(contents[i].data(), contents[i].size());
    data->paths.emplace_back(path);
  }
}

const std::vector<base::FilePath>&
DeepScanningBrowserTestBase::created_file_paths() const {
  return created_file_paths_;
}

}  // namespace safe_browsing
