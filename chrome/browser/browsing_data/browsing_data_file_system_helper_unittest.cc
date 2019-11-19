// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::BrowserThread;

namespace {

// Shorter names for storage::* constants.
const storage::FileSystemType kTemporary = storage::kFileSystemTypeTemporary;
const storage::FileSystemType kPersistent = storage::kFileSystemTypePersistent;

// We'll use these three distinct origins for testing, both as strings and as
// Origins in appropriate contexts.
const char kTestOrigin1[] = "http://host1:1";
const char kTestOrigin2[] = "http://host2:2";
const char kTestOrigin3[] = "http://host3:3";

// Extensions and Devtools should be ignored.
const char kTestOriginExt[] = "chrome-extension://abcdefghijklmnopqrstuvwxyz";
const char kTestOriginDevTools[] = "devtools://abcdefghijklmnopqrstuvw";

const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));
const url::Origin kOrigin2 = url::Origin::Create(GURL(kTestOrigin2));
const url::Origin kOrigin3 = url::Origin::Create(GURL(kTestOrigin3));
const url::Origin kOriginExt = url::Origin::Create(GURL(kTestOriginExt));
const url::Origin kOriginDevTools =
    url::Origin::Create(GURL(kTestOriginDevTools));

// TODO(mkwst): Update this size once the discussion in http://crbug.com/86114
// is concluded.
const int kEmptyFileSystemSize = 0;

using FileSystemInfoList =
    std::list<BrowsingDataFileSystemHelper::FileSystemInfo>;

// The FileSystem APIs are all asynchronous; this testing class wraps up the
// boilerplate code necessary to deal with waiting for responses. In a nutshell,
// any async call whose response we want to test ought to create a base::RunLoop
// instance to be followed by a call to BlockUntilQuit(), which will
// (shockingly!) block until Quit() is called on the RunLoop.
class BrowsingDataFileSystemHelperTest : public testing::Test {
 public:
  BrowsingDataFileSystemHelperTest() {
    profile_.reset(new TestingProfile());
    auto* file_system_context =
        BrowserContext::GetDefaultStoragePartition(profile_.get())
            ->GetFileSystemContext();
    helper_ = BrowsingDataFileSystemHelper::Create(file_system_context);
    content::RunAllTasksUntilIdle();
    canned_helper_ =
        new CannedBrowsingDataFileSystemHelper(file_system_context);
  }
  ~BrowsingDataFileSystemHelperTest() override {
    // Avoid memory leaks.
    profile_.reset();
    content::RunAllTasksUntilIdle();
  }

  TestingProfile* GetProfile() {
    return profile_.get();
  }

  // Blocks on the run_loop quits.
  void BlockUntilQuit(base::RunLoop* run_loop) {
    run_loop->Run();                  // Won't return until Quit().
    content::RunAllTasksUntilIdle();  // Flush other runners.
  }

  // Callback that should be executed in response to
  // storage::FileSystemContext::OpenFileSystem.
  void OpenFileSystemCallback(base::RunLoop* run_loop,
                              const GURL& root,
                              const std::string& name,
                              base::File::Error error) {
    open_file_system_result_ = error;
    run_loop->Quit();
  }

  bool OpenFileSystem(const url::Origin& origin,
                      storage::FileSystemType type,
                      storage::OpenFileSystemMode open_mode) {
    base::RunLoop run_loop;
    BrowserContext::GetDefaultStoragePartition(profile_.get())
        ->GetFileSystemContext()
        ->OpenFileSystem(
            origin.GetURL(), type, open_mode,
            base::Bind(
                &BrowsingDataFileSystemHelperTest::OpenFileSystemCallback,
                base::Unretained(this), &run_loop));
    BlockUntilQuit(&run_loop);
    return open_file_system_result_ == base::File::FILE_OK;
  }

  // Calls storage::FileSystemContext::OpenFileSystem with
  // OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT flag
  // to verify the existence of a file system for a specified type and origin,
  // blocks until a response is available, then returns the result
  // synchronously to it's caller.
  bool FileSystemContainsOriginAndType(const url::Origin& origin,
                                       storage::FileSystemType type) {
    return OpenFileSystem(
        origin, type, storage::OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT);
  }

  // Callback that should be executed in response to StartFetching(), and stores
  // found file systems locally so that they are available via GetFileSystems().
  void CallbackStartFetching(
      base::RunLoop* run_loop,
      const std::list<BrowsingDataFileSystemHelper::FileSystemInfo>&
          file_system_info_list) {
    file_system_info_list_.reset(
        new std::list<BrowsingDataFileSystemHelper::FileSystemInfo>(
            file_system_info_list));
    run_loop->Quit();
  }

  // Calls StartFetching() on the test's BrowsingDataFileSystemHelper
  // object, then blocks until the callback is executed.
  void FetchFileSystems() {
    base::RunLoop run_loop;
    helper_->StartFetching(
        base::Bind(&BrowsingDataFileSystemHelperTest::CallbackStartFetching,
                   base::Unretained(this), &run_loop));
    BlockUntilQuit(&run_loop);
  }

  // Calls StartFetching() on the test's CannedBrowsingDataFileSystemHelper
  // object, then blocks until the callback is executed.
  void FetchCannedFileSystems() {
    base::RunLoop run_loop;
    canned_helper_->StartFetching(
        base::Bind(&BrowsingDataFileSystemHelperTest::CallbackStartFetching,
                   base::Unretained(this), &run_loop));
    BlockUntilQuit(&run_loop);
  }

  // Sets up kOrigin1 with a temporary file system, kOrigin2 with a persistent
  // file system, and kOrigin3 with both.
  virtual void PopulateTestFileSystemData() {
    CreateDirectoryForOriginAndType(kOrigin1, kTemporary);
    CreateDirectoryForOriginAndType(kOrigin2, kPersistent);
    CreateDirectoryForOriginAndType(kOrigin3, kTemporary);
    CreateDirectoryForOriginAndType(kOrigin3, kPersistent);

    EXPECT_FALSE(FileSystemContainsOriginAndType(kOrigin1, kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin1, kTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin2, kPersistent));
    EXPECT_FALSE(FileSystemContainsOriginAndType(kOrigin2, kTemporary));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin3, kPersistent));
    EXPECT_TRUE(FileSystemContainsOriginAndType(kOrigin3, kTemporary));
  }

  // Calls OpenFileSystem with OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT
  // to create a filesystem of a given type for a specified origin.
  void CreateDirectoryForOriginAndType(const url::Origin& origin,
                                       storage::FileSystemType type) {
    OpenFileSystem(
        origin, type, storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT);
    EXPECT_EQ(base::File::FILE_OK, open_file_system_result_);
  }

  // Returns a list of the FileSystemInfo objects gathered in the most recent
  // call to StartFetching().
  FileSystemInfoList* GetFileSystems() {
    return file_system_info_list_.get();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  // Temporary storage to pass information back from callbacks.
  base::File::Error open_file_system_result_;
  std::unique_ptr<FileSystemInfoList> file_system_info_list_;

  scoped_refptr<BrowsingDataFileSystemHelper> helper_;
  scoped_refptr<CannedBrowsingDataFileSystemHelper> canned_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataFileSystemHelperTest);
};

// Verifies that the BrowsingDataFileSystemHelper correctly finds the test file
// system data, and that each file system returned contains the expected data.
TEST_F(BrowsingDataFileSystemHelperTest, FetchData) {
  PopulateTestFileSystemData();

  FetchFileSystems();

  EXPECT_EQ(3UL, file_system_info_list_->size());

  // Order is arbitrary, verify all three origins.
  bool test_hosts_found[3] = {false, false, false};
  for (const auto& info : *file_system_info_list_) {
    if (info.origin == kOrigin1) {
      EXPECT_FALSE(test_hosts_found[0]);
      test_hosts_found[0] = true;
      EXPECT_FALSE(base::Contains(info.usage_map, kPersistent));
      EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize,
                info.usage_map.at(storage::kFileSystemTypeTemporary));
    } else if (info.origin == kOrigin2) {
      EXPECT_FALSE(test_hosts_found[1]);
      test_hosts_found[1] = true;
      EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
      EXPECT_FALSE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_map.at(kPersistent));
    } else if (info.origin == kOrigin3) {
      EXPECT_FALSE(test_hosts_found[2]);
      test_hosts_found[2] = true;
      EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
      EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_map.at(kPersistent));
      EXPECT_EQ(kEmptyFileSystemSize, info.usage_map.at(kTemporary));
    } else {
      ADD_FAILURE() << info.origin.Serialize() << " isn't an origin we added.";
    }
  }
  for (size_t i = 0; i < base::size(test_hosts_found); i++) {
    EXPECT_TRUE(test_hosts_found[i]);
  }
}

// Verifies that the BrowsingDataFileSystemHelper correctly deletes file
// systems via DeleteFileSystemOrigin().
TEST_F(BrowsingDataFileSystemHelperTest, DeleteData) {
  PopulateTestFileSystemData();

  helper_->DeleteFileSystemOrigin(kOrigin1);
  helper_->DeleteFileSystemOrigin(kOrigin2);

  FetchFileSystems();

  EXPECT_EQ(1UL, file_system_info_list_->size());
  BrowsingDataFileSystemHelper::FileSystemInfo info =
      *(file_system_info_list_->begin());
  EXPECT_EQ(kOrigin3, info.origin);
  EXPECT_TRUE(base::Contains(info.usage_map, kPersistent));
  EXPECT_TRUE(base::Contains(info.usage_map, kTemporary));
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_map[kPersistent]);
  EXPECT_EQ(kEmptyFileSystemSize, info.usage_map[kTemporary]);
}

// Verifies that the CannedBrowsingDataFileSystemHelper correctly reports
// whether or not it currently contains file systems.
TEST_F(BrowsingDataFileSystemHelperTest, Empty) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(kOrigin1);
  ASSERT_FALSE(canned_helper_->empty());
  canned_helper_->Reset();
  ASSERT_TRUE(canned_helper_->empty());
}

// Verifies that AddFileSystem correctly adds file systems. The canned helper
// does not record usage size.
TEST_F(BrowsingDataFileSystemHelperTest, CannedAddFileSystem) {
  canned_helper_->Add(kOrigin1);
  canned_helper_->Add(kOrigin2);

  FetchCannedFileSystems();

  EXPECT_EQ(2U, file_system_info_list_->size());
  auto info = file_system_info_list_->begin();
  EXPECT_EQ(kOrigin1, info->origin);
  EXPECT_FALSE(base::Contains(info->usage_map, kPersistent));
  EXPECT_FALSE(base::Contains(info->usage_map, kTemporary));

  info++;
  EXPECT_EQ(kOrigin2, info->origin);
  EXPECT_FALSE(base::Contains(info->usage_map, kPersistent));
  EXPECT_FALSE(base::Contains(info->usage_map, kTemporary));
}

// Verifies that the CannedBrowsingDataFileSystemHelper correctly ignores
// extension and devtools schemes.
TEST_F(BrowsingDataFileSystemHelperTest, IgnoreExtensionsAndDevTools) {
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(kOriginExt);
  ASSERT_TRUE(canned_helper_->empty());
  canned_helper_->Add(kOriginDevTools);
  ASSERT_TRUE(canned_helper_->empty());
}

}  // namespace
