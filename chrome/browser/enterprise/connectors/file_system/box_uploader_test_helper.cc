// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/box_uploader_test_helper.h"

#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"

namespace {
std::string GetTestName() {
  // Gets information about the currently running test.
  // Do NOT delete the returned object - it's managed by the UnitTest class.
  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  std::string name(test_info->test_suite_name());
  name += ".";
  name += test_info->name();
  return name;
}
}  // namespace

namespace enterprise_connectors {

////////////////////////////////////////////////////////////////////////////////
// DownloadItemForTest
////////////////////////////////////////////////////////////////////////////////

DownloadItemForTest::DownloadItemForTest(
    base::FilePath::StringPieceType file_name) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  file_path_ = temp_dir_.GetPath().Append(file_name);
  CHECK_EQ(file_path_.FinalExtension(), FILE_PATH_LITERAL(".crdownload"));
  SetTargetFilePath(file_path_.RemoveFinalExtension());
}

const base::FilePath& DownloadItemForTest::GetFullPath() const {
  return file_path_;
}

////////////////////////////////////////////////////////////////////////////////
// BoxUploaderTestBase
////////////////////////////////////////////////////////////////////////////////

BoxUploaderTestBase::BoxUploaderTestBase(
    base::FilePath::StringPieceType file_name)
    : test_item_(file_name),
      url_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {
  EXPECT_TRUE(profile_manager_.SetUp());
  prefs_ = profile_manager_.CreateTestingProfile("test-user")->GetPrefs();
}

BoxUploaderTestBase::~BoxUploaderTestBase() = default;

void BoxUploaderTestBase::AddFetchResult(const std::string& url,
                                         net::HttpStatusCode code,
                                         std::string body) {
  test_url_loader_factory_.AddResponse(url, std::move(body), code);
}

base::FilePath BoxUploaderTestBase::GetFilePath() const {
  return test_item_.GetFullPath();
}

void BoxUploaderTestBase::CreateTemporaryFile() {
  auto path = test_item_.GetFullPath();
  ASSERT_FALSE(path.empty());
  ASSERT_TRUE(base::WriteFile(path, GetTestName())) << path;
}

void BoxUploaderTestBase::InitUploader(BoxUploader* uploader) {
  ASSERT_TRUE(uploader);
  uploader->Init(base::BindRepeating(&BoxUploaderTestBase::AuthenticationRetry,
                                     base::Unretained(this)),
                 base::BindOnce(&BoxUploaderTestBase::UploaderFinished,
                                base::Unretained(this)),
                 prefs_);
}

void BoxUploaderTestBase::InitFolderIdInPrefs(std::string folder_id) {
  prefs_->SetString(kFileSystemUploadFolderIdPref, folder_id);
}

void BoxUploaderTestBase::RunWithQuitClosure() {
  run_loop_ = std::make_unique<base::RunLoop>();
  quit_closure_ = run_loop_->QuitClosure();
  run_loop_->Run();
}

void BoxUploaderTestBase::Quit() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void BoxUploaderTestBase::AuthenticationRetry() {
  ++authentication_retry_;
  Quit();
}

void BoxUploaderTestBase::UploaderFinished(bool success) {
  download_thread_cb_called_ = true;
  upload_success_ = success;
  Quit();
}

////////////////////////////////////////////////////////////////////////////////
// MockApiCallFlow
////////////////////////////////////////////////////////////////////////////////
MockApiCallFlow::MockApiCallFlow() = default;
MockApiCallFlow::~MockApiCallFlow() = default;

}  // namespace enterprise_connectors
