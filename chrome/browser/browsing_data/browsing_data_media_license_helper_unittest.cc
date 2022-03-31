// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_media_license_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using content::BrowserContext;
using content::BrowserThread;
using content::StorageUsageInfo;

namespace {

const char kWidevineCdmPluginId[] = "application_x-ppapi-widevine-cdm";
const char kClearKeyCdmPluginId[] = "application_x-ppapi-clearkey-cdm";

// The FileSystem APIs are all asynchronous; this testing class wraps up the
// boilerplate code necessary to deal with waiting for responses. In a nutshell,
// any async call whose response we want to test ought to be followed by a call
// to BlockUntilNotified(), which will block until Notify() is called.
class BrowsingDataMediaLicenseHelperTest : public testing::Test {
 public:
  BrowsingDataMediaLicenseHelperTest() {
    now_ = base::Time::Now();
    profile_ = std::make_unique<TestingProfile>();
    filesystem_context_ =
        profile_->GetDefaultStoragePartition()->GetFileSystemContext();
    helper_ = BrowsingDataMediaLicenseHelper::Create(filesystem_context_);
    base::RunLoop().RunUntilIdle();
  }

  BrowsingDataMediaLicenseHelperTest(
      const BrowsingDataMediaLicenseHelperTest&) = delete;
  BrowsingDataMediaLicenseHelperTest& operator=(
      const BrowsingDataMediaLicenseHelperTest&) = delete;

  ~BrowsingDataMediaLicenseHelperTest() override {
    // Avoid memory leaks.
    profile_.reset();
    base::RunLoop().RunUntilIdle();
  }

  // Calls StartFetching() on the test's BrowsingDataMediaLicenseHelper object,
  // then stores found file systems locally so that they are available via
  // GetFileSystems().
  void FetchMediaLicenses() {
    base::test::TestFuture<std::list<StorageUsageInfo>> future;
    helper_->StartFetching(
        future.GetCallback<const std::list<StorageUsageInfo>&>());

    media_license_info_list_ =
        std::make_unique<std::list<StorageUsageInfo>>(future.Take());
  }

  // Add some files to the PluginPrivateFileSystem. They are created as follows:
  //   |origin1| - ClearKey - 1 file - timestamp 10 days ago
  //   |origin2| - Widevine - 2 files - timestamps now and 60 days ago
  //   |origin3| - Widevine - 2 files - timestamps 20 and 30 days ago
  virtual void PopulateTestMediaLicenseData(const url::Origin& origin1,
                                            const url::Origin& origin2,
                                            const url::Origin& origin3) {
    const base::Time ten_days_ago = now_ - base::Days(10);
    const base::Time twenty_days_ago = now_ - base::Days(20);
    const base::Time thirty_days_ago = now_ - base::Days(30);
    const base::Time sixty_days_ago = now_ - base::Days(60);

    std::string clearkey_fsid = CreateFileSystem(kClearKeyCdmPluginId, origin1);
    storage::FileSystemURL clearkey_file =
        CreateFile(origin1, clearkey_fsid, "foo");
    SetFileTimestamp(clearkey_file, ten_days_ago);

    std::string widevine_fsid = CreateFileSystem(kWidevineCdmPluginId, origin2);
    storage::FileSystemURL widevine_file1 =
        CreateFile(origin2, widevine_fsid, "bar1");
    storage::FileSystemURL widevine_file2 =
        CreateFile(origin2, widevine_fsid, "bar2");
    SetFileTimestamp(widevine_file1, now_);
    SetFileTimestamp(widevine_file2, sixty_days_ago);

    std::string widevine_fsid2 =
        CreateFileSystem(kWidevineCdmPluginId, origin3);
    storage::FileSystemURL widevine_file3 =
        CreateFile(origin3, widevine_fsid2, "test1");
    storage::FileSystemURL widevine_file4 =
        CreateFile(origin3, widevine_fsid2, "test2");
    SetFileTimestamp(widevine_file3, twenty_days_ago);
    SetFileTimestamp(widevine_file4, thirty_days_ago);
  }

  const base::Time Now() { return now_; }

  void DeleteMediaLicenseOrigin(const url::Origin& origin) {
    helper_->DeleteMediaLicenseOrigin(origin);
  }

  std::list<StorageUsageInfo>* ReturnedMediaLicenseInfo() const {
    return media_license_info_list_.get();
  }

 private:
  // Creates a PluginPrivateFileSystem for the |plugin_name| and |origin|
  // provided. Returns the file system ID for the created
  // PluginPrivateFileSystem.
  std::string CreateFileSystem(const std::string& plugin_name,
                               const url::Origin& origin) {
    std::string fsid =
        storage::IsolatedContext::GetInstance()
            ->RegisterFileSystemForVirtualPath(
                storage::kFileSystemTypePluginPrivate,
                storage::kPluginPrivateRootName, base::FilePath());
    EXPECT_TRUE(storage::ValidateIsolatedFileSystemId(fsid));

    base::test::TestFuture<base::File::Error> future;
    filesystem_context_->OpenPluginPrivateFileSystem(
        origin, storage::kFileSystemTypePluginPrivate, fsid, plugin_name,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT, future.GetCallback());
    const base::File::Error result = future.Get();
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    return fsid;
  }

  // Creates a file named |file_name| in the PluginPrivateFileSystem identified
  // by |origin| and |fsid|. The file is empty (so size = 0). Returns the URL
  // for the created file. The file must not already exist or the test will
  // fail.
  storage::FileSystemURL CreateFile(const url::Origin& origin,
                                    const std::string& fsid,
                                    const std::string& file_name) {
    std::string root = storage::GetIsolatedFileSystemRootURIString(
        origin.GetURL(), fsid, storage::kPluginPrivateRootName);
    storage::FileSystemURL file_url =
        filesystem_context_->CrackURLInFirstPartyContext(
            GURL(root + file_name));
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context(
        std::make_unique<storage::FileSystemOperationContext>(
            filesystem_context_));
    operation_context->set_allowed_bytes_growth(
        storage::QuotaManager::kNoLimit);

    base::test::TestFuture<base::File::Error, bool> future;
    file_util->EnsureFileExists(std::move(operation_context), file_url,
                                future.GetCallback());
    const base::File::Error result = future.Get<0>();
    const bool created = future.Get<1>();
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
    EXPECT_TRUE(created);
    return file_url;
  }

  // Sets the last_access_time and last_modified_time to |time_stamp| on the
  // file specified by |file_url|. The file must already exist.
  void SetFileTimestamp(const storage::FileSystemURL& file_url,
                        const base::Time& time_stamp) {
    storage::AsyncFileUtil* file_util = filesystem_context_->GetAsyncFileUtil(
        storage::kFileSystemTypePluginPrivate);
    std::unique_ptr<storage::FileSystemOperationContext> operation_context(
        std::make_unique<storage::FileSystemOperationContext>(
            filesystem_context_));

    base::test::TestFuture<base::File::Error> future;
    file_util->Touch(std::move(operation_context), file_url, time_stamp,
                     time_stamp, future.GetCallback());
    const base::File::Error result = future.Get();
    EXPECT_EQ(base::File::FILE_OK, result) << base::File::ErrorToString(result);
  }
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  scoped_refptr<BrowsingDataMediaLicenseHelper> helper_;

  // Keep a fixed "now" so that we can compare timestamps.
  base::Time now_;

  // We don't own this pointer.
  raw_ptr<storage::FileSystemContext> filesystem_context_;

  // Storage to pass information back from callbacks.
  std::unique_ptr<std::list<StorageUsageInfo>> media_license_info_list_;
};

// Verifies that the BrowsingDataMediaLicenseHelper correctly handles an empty
// filesystem.
TEST_F(BrowsingDataMediaLicenseHelperTest, Empty) {
  FetchMediaLicenses();
  EXPECT_EQ(0u, ReturnedMediaLicenseInfo()->size());
}

// Verifies that the BrowsingDataMediaLicenseHelper correctly finds the test
// data, and that each media license returned contains the expected data.
TEST_F(BrowsingDataMediaLicenseHelperTest, FetchData) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1"));
  PopulateTestMediaLicenseData(kOrigin1, kOrigin2, kOrigin3);

  FetchMediaLicenses();
  EXPECT_EQ(3u, ReturnedMediaLicenseInfo()->size());

  // Order is arbitrary, verify both origins.
  bool test_hosts_found[] = {false, false, false};
  for (const auto& info : *ReturnedMediaLicenseInfo()) {
    if (info.origin == kOrigin1) {
      EXPECT_FALSE(test_hosts_found[0]);
      test_hosts_found[0] = true;
      EXPECT_EQ(0u, info.total_size_bytes);
      // Single file for |origin1| should be 10 days ago.
      EXPECT_EQ(10, (Now() - info.last_modified).InDays());
    } else if (info.origin == kOrigin2) {
      EXPECT_FALSE(test_hosts_found[1]);
      test_hosts_found[1] = true;
      EXPECT_EQ(0u, info.total_size_bytes);
      // Files for |origin2| are now and 60 days ago, so it should report now.
      EXPECT_EQ(0, (Now() - info.last_modified).InDays());
    } else if (info.origin == kOrigin3) {
      EXPECT_FALSE(test_hosts_found[2]);
      test_hosts_found[2] = true;
      EXPECT_EQ(0u, info.total_size_bytes);
      // Files for |origin3| are 20 and 30 days ago, so it should report 20.
      EXPECT_EQ(20, (Now() - info.last_modified).InDays());
    } else {
      ADD_FAILURE() << info.origin.GetURL().spec()
                    << " isn't an origin we added.";
    }
  }
  for (const auto found : test_hosts_found) {
    EXPECT_TRUE(found);
  }
}

// Verifies that the BrowsingDataMediaLicenseHelper correctly deletes media
// licenses via DeleteMediaLicenseOrigin().
TEST_F(BrowsingDataMediaLicenseHelperTest, DeleteData) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://host1:1"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://host2:1"));
  const url::Origin kOrigin3 = url::Origin::Create(GURL("http://host3:1"));
  PopulateTestMediaLicenseData(kOrigin1, kOrigin2, kOrigin3);

  DeleteMediaLicenseOrigin(kOrigin1);
  DeleteMediaLicenseOrigin(kOrigin2);

  FetchMediaLicenses();
  EXPECT_EQ(1u, ReturnedMediaLicenseInfo()->size());

  StorageUsageInfo info = *(ReturnedMediaLicenseInfo()->begin());
  EXPECT_EQ(kOrigin3, info.origin);
  EXPECT_EQ(0u, info.total_size_bytes);
  EXPECT_EQ(20, (Now() - info.last_modified).InDays());
}

}  // namespace
