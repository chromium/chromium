// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/obfuscation_rename_handler.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_obfuscation {

class ObfuscationRenameHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    staged_path_ = temp_dir_.GetPath().AppendASCII("staged.txt");
    target_path_ = temp_dir_.GetPath().AppendASCII("target.txt");
    ASSERT_TRUE(base::WriteFile(staged_path_, "test content"));
  }

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  base::FilePath staged_path_;
  base::FilePath target_path_;
};

TEST_F(ObfuscationRenameHandlerTest,
       CreateIfNeededReturnsNullWithoutObfuscation) {
  download::MockDownloadItem item;
  EXPECT_EQ(nullptr, ObfuscationRenameHandler::CreateIfNeeded(&item));
}

TEST_F(ObfuscationRenameHandlerTest, CreateIfNeededWithObfuscation) {
  download::MockDownloadItem item;
  auto obfuscation_data =
      std::make_unique<DownloadObfuscationData>(/*is_obfuscated=*/true);
  obfuscation_data->original_target_path = target_path_;
  item.SetUserData(DownloadObfuscationData::kUserDataKey,
                   std::move(obfuscation_data));

  auto handler = ObfuscationRenameHandler::CreateIfNeeded(&item);
  EXPECT_NE(nullptr, handler);
}

TEST_F(ObfuscationRenameHandlerTest, StartMovesFileToTarget) {
  download::MockDownloadItem item;
  auto obfuscation_data =
      std::make_unique<DownloadObfuscationData>(/*is_obfuscated=*/true);
  obfuscation_data->original_target_path = target_path_;
  item.SetUserData(DownloadObfuscationData::kUserDataKey,
                   std::move(obfuscation_data));
  ON_CALL(item, GetTargetFilePath())
      .WillByDefault(testing::ReturnRef(staged_path_));

  auto handler = ObfuscationRenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler);

  base::RunLoop run_loop;
  handler->Start(
      base::DoNothing(),
      base::BindLambdaForTesting([&](download::DownloadInterruptReason reason,
                                     const base::FilePath& path) {
        EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE, reason);
        EXPECT_EQ(target_path_, path);
        EXPECT_FALSE(base::PathExists(staged_path_));
        EXPECT_TRUE(base::PathExists(target_path_));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ObfuscationRenameHandlerTest, StartMoveFails) {
  download::MockDownloadItem item;
  auto obfuscation_data =
      std::make_unique<DownloadObfuscationData>(/*is_obfuscated=*/true);
  obfuscation_data->original_target_path = target_path_;
  item.SetUserData(DownloadObfuscationData::kUserDataKey,
                   std::move(obfuscation_data));
  ON_CALL(item, GetTargetFilePath())
      .WillByDefault(testing::ReturnRef(staged_path_));

  // Delete the staged file so MoveAndOverwrite fails.
  ASSERT_TRUE(base::DeleteFile(staged_path_));

  auto handler = ObfuscationRenameHandler::CreateIfNeeded(&item);
  ASSERT_NE(nullptr, handler);

  base::RunLoop run_loop;
  handler->Start(
      base::DoNothing(),
      base::BindLambdaForTesting([&](download::DownloadInterruptReason reason,
                                     const base::FilePath& path) {
        EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, reason);
        EXPECT_EQ(target_path_, path);
        EXPECT_FALSE(base::PathExists(target_path_));
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace enterprise_obfuscation
