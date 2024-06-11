// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/skyvault_rename_handler.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/mock_download_item.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class SkyvaultRenameHandlerTest : public testing::Test {
 public:
  SkyvaultRenameHandlerTest() = default;

  SkyvaultRenameHandlerTest(const SkyvaultRenameHandlerTest&) = delete;
  SkyvaultRenameHandlerTest& operator=(const SkyvaultRenameHandlerTest&) =
      delete;

  ~SkyvaultRenameHandlerTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
  std::unique_ptr<download::MockDownloadItem> download_item_ =
      std::make_unique<::testing::NiceMock<download::MockDownloadItem>>();
};

TEST_F(SkyvaultRenameHandlerTest, SuccessfulUploadToGDrive) {
  SkyvaultRenameHandler rename_handler(
      profile_.get(), SkyvaultRenameHandler::CloudProvider::kGoogleDrive,
      download_item_.get());

  auto file_path = base::FilePath(FILE_PATH_LITERAL("foo"));
  EXPECT_CALL(*download_item_, GetFullPath())
      .WillRepeatedly(testing::ReturnRefOfCopy(file_path));
  EXPECT_CALL(*download_item_, GetTargetFilePath())
      .WillRepeatedly(testing::ReturnRefOfCopy(file_path));

  base::MockCallback<SkyvaultRenameHandler::ProgressCallback> progress_callback;
  base::MockCallback<SkyvaultRenameHandler::RenameCallback> rename_callback;
  rename_handler.StartForTesting(progress_callback.Get(),
                                 rename_callback.Get());

  EXPECT_CALL(progress_callback, Run(1234, 0));
  rename_handler.OnProgressUpdate(1234);

  EXPECT_CALL(rename_callback,
              Run(download::DOWNLOAD_INTERRUPT_REASON_NONE, file_path));
  rename_handler.OnDriveUploadDone(true);
}

TEST_F(SkyvaultRenameHandlerTest, FailedUploadToGDrive) {
  SkyvaultRenameHandler rename_handler(
      profile_.get(), SkyvaultRenameHandler::CloudProvider::kGoogleDrive,
      download_item_.get());

  auto file_path = base::FilePath(FILE_PATH_LITERAL("foo"));
  EXPECT_CALL(*download_item_, GetFullPath())
      .WillRepeatedly(testing::ReturnRefOfCopy(file_path));
  EXPECT_CALL(*download_item_, GetTargetFilePath())
      .WillRepeatedly(testing::ReturnRefOfCopy(file_path));

  base::MockCallback<SkyvaultRenameHandler::ProgressCallback> progress_callback;
  base::MockCallback<SkyvaultRenameHandler::RenameCallback> rename_callback;
  rename_handler.StartForTesting(progress_callback.Get(),
                                 rename_callback.Get());

  EXPECT_CALL(progress_callback, Run(378, 0));
  rename_handler.OnProgressUpdate(378);

  EXPECT_CALL(rename_callback,
              Run(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, file_path));
  rename_handler.OnDriveUploadDone(false);
}

TEST_F(SkyvaultRenameHandlerTest, SuccessfulUploadToOneDrive) {
  SkyvaultRenameHandler rename_handler(
      profile_.get(), SkyvaultRenameHandler::CloudProvider::kOneDrive,
      download_item_.get());

  EXPECT_CALL(*download_item_, GetFullPath())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));

  base::MockCallback<SkyvaultRenameHandler::ProgressCallback> progress_callback;
  base::MockCallback<SkyvaultRenameHandler::RenameCallback> rename_callback;
  rename_handler.StartForTesting(progress_callback.Get(),
                                 rename_callback.Get());

  EXPECT_CALL(progress_callback, Run(1234, 0));
  rename_handler.OnProgressUpdate(1234);

  EXPECT_CALL(rename_callback,
              Run(download::DOWNLOAD_INTERRUPT_REASON_NONE, base::FilePath()));
  rename_handler.OnOneDriveUploadDone(true, storage::FileSystemURL());
}

TEST_F(SkyvaultRenameHandlerTest, FailedUploadToOneDrive) {
  SkyvaultRenameHandler rename_handler(
      profile_.get(), SkyvaultRenameHandler::CloudProvider::kOneDrive,
      download_item_.get());

  EXPECT_CALL(*download_item_, GetFullPath())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(base::FilePath(FILE_PATH_LITERAL("foo"))));

  base::MockCallback<SkyvaultRenameHandler::ProgressCallback> progress_callback;
  base::MockCallback<SkyvaultRenameHandler::RenameCallback> rename_callback;
  rename_handler.StartForTesting(progress_callback.Get(),
                                 rename_callback.Get());

  EXPECT_CALL(progress_callback, Run(99, 0));
  rename_handler.OnProgressUpdate(99);

  EXPECT_CALL(
      rename_callback,
      Run(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, base::FilePath()));
  rename_handler.OnOneDriveUploadDone(false, storage::FileSystemURL());
}

}  // namespace policy
