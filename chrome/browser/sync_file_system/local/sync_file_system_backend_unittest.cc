// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"

#include <memory>

#include "base/files/file.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/local/canned_syncable_file_system.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "url/gurl.h"

namespace sync_file_system {

class SyncFileSystemBackendTest : public testing::Test {
 public:
  SyncFileSystemBackendTest()
      : in_memory_env_(leveldb_chrome::NewMemEnv("SyncFileSystemBackendTest")) {
  }

  SyncFileSystemBackendTest(const SyncFileSystemBackendTest&) = delete;
  SyncFileSystemBackendTest& operator=(const SyncFileSystemBackendTest&) =
      delete;

 protected:
  std::unique_ptr<CannedSyncableFileSystem> CreateFileSystem(
      const GURL& origin) {
    auto file_system = std::make_unique<CannedSyncableFileSystem>(
        origin, in_memory_env_.get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get());
    file_system->SetUp();
    return file_system;
  }

  base::File::Error OpenFileSystemForOrigin(const GURL& origin) {
    auto file_system = CreateFileSystem(origin);
    base::File::Error result = file_system->OpenFileSystem();
    file_system->TearDown();
    return result;
  }

  base::File::Error CreateFileSystemOperationForOrigin(const GURL& origin) {
    auto file_system = CreateFileSystem(origin);
    base::File::Error result = base::File::FILE_OK;
    file_system->backend()->CreateFileSystemOperation(
        storage::OperationType::kCreateFile,
        CreateSyncableFileSystemURL(origin, base::FilePath()),
        file_system->file_system_context(), &result);
    file_system->TearDown();
    return result;
  }

  void TearDown() override { RevokeSyncableFileSystem(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<leveldb::Env> in_memory_env_;
};

struct SchemeTestParam {
  const char* scheme_name;
  const char* url_string;
  base::File::Error expected_error;
};

class SyncFileSystemBackendSchemeTest
    : public SyncFileSystemBackendTest,
      public ::testing::WithParamInterface<SchemeTestParam> {};

TEST_P(SyncFileSystemBackendSchemeTest, OpenFileSystem) {
  const SchemeTestParam& param = GetParam();
  EXPECT_EQ(param.expected_error,
            OpenFileSystemForOrigin(GURL(param.url_string)));
}

TEST_P(SyncFileSystemBackendSchemeTest, CreateFileSystemOperation) {
  const SchemeTestParam& param = GetParam();
  EXPECT_EQ(param.expected_error,
            CreateFileSystemOperationForOrigin(GURL(param.url_string)));
}

const SchemeTestParam kSchemeTestParams[] = {
    {"chrome_extension", "chrome-extension://example/", base::File::FILE_OK},
    {"http", "http://example.com/", base::File::FILE_ERROR_SECURITY},
    {"https", "https://example.com/", base::File::FILE_ERROR_SECURITY},
    {"file", "file:///foo/bar", base::File::FILE_ERROR_SECURITY},
    {"chrome", "chrome://settings/", base::File::FILE_ERROR_SECURITY},
    {"ftp", "ftp://example.com/", base::File::FILE_ERROR_SECURITY},
};

INSTANTIATE_TEST_SUITE_P(
    SyncFileSystemBackend,
    SyncFileSystemBackendSchemeTest,
    ::testing::ValuesIn(kSchemeTestParams),
    [](const ::testing::TestParamInfo<SchemeTestParam>& info) {
      return info.param.scheme_name;
    });

}  // namespace sync_file_system
