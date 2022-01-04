// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

#include <vector>

#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

ino_t GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return 0;
  return file_stats.st_ino;
}

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42", sizeof("42")) == sizeof("42");
}

}  // namespace

class DlpFilesControllerTest : public testing::Test {
 protected:
  DlpFilesControllerTest() : files_controller_(&rules_manager_) {}

  DlpFilesControllerTest(const DlpFilesControllerTest&) = delete;
  DlpFilesControllerTest& operator=(const DlpFilesControllerTest&) = delete;

  ~DlpFilesControllerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));
  }

  content::BrowserTaskEnvironment task_environment_;

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  testing::StrictMock<MockDlpRulesManager> rules_manager_;
  DlpFilesController files_controller_;
};

TEST_F(DlpFilesControllerTest, GetDisallowedTransfers) {
  chromeos::DlpClient::InitializeFake();
  base::MockCallback<chromeos::DlpClient::AddFileCallback> add_file_cb;

  EXPECT_CALL(add_file_cb, Run(testing::_)).Times(3);

  const base::FilePath path = temp_dir_.GetPath();

  const base::FilePath file1 = path.AppendASCII("test1.txt");
  ASSERT_TRUE(CreateDummyFile(file1));
  dlp::AddFileRequest add_file_req1;
  add_file_req1.set_file_path(file1.value());
  add_file_req1.set_source_url("example1.com");
  chromeos::DlpClient::Get()->AddFile(add_file_req1, add_file_cb.Get());

  const base::FilePath file2 = path.AppendASCII("test2.txt");
  ASSERT_TRUE(CreateDummyFile(file2));
  dlp::AddFileRequest add_file_req2;
  add_file_req2.set_file_path(file2.value());
  add_file_req2.set_source_url("example2.com");
  chromeos::DlpClient::Get()->AddFile(add_file_req2, add_file_cb.Get());

  const base::FilePath file3 = path.AppendASCII("test3.txt");
  ASSERT_TRUE(CreateDummyFile(file3));
  dlp::AddFileRequest add_file_req3;
  add_file_req3.set_file_path(file3.value());
  add_file_req3.set_source_url("example3.com");
  chromeos::DlpClient::Get()->AddFile(add_file_req3, add_file_cb.Get());

  testing::Mock::VerifyAndClearExpectations(&add_file_cb);

  storage::FileSystemURL file_url1 = CreateFileSystemURL(file1.value());
  storage::FileSystemURL file_url2 = CreateFileSystemURL(file2.value());
  storage::FileSystemURL file_url3 = CreateFileSystemURL(file3.value());
  std::vector<storage::FileSystemURL> transferred_files(
      {file_url1, file_url2, file_url3});
  std::vector<storage::FileSystemURL> disallowed_files({file_url1, file_url3});
  base::MockCallback<DlpFilesController::GetDisallowedTransfersCallback>
      disallowed_transfers_cb;

  EXPECT_CALL(disallowed_transfers_cb, Run(disallowed_files));
  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  base::RunLoop run_loop;
  base::RepeatingClosure quit_closure = run_loop.QuitClosure();

  files_controller_.GetDisallowedTransfers(
      transferred_files, CreateFileSystemURL("Downloads"),
      disallowed_transfers_cb.Get().Then(std::move(quit_closure)));

  run_loop.Run();
}

}  // namespace policy
