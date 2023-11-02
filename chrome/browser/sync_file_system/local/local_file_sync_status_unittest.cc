// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using storage::FileSystemURL;

namespace sync_file_system {

namespace {

const char kParent[] = "filesystem:http://foo.com/test/dir a";
const char kFile[]   = "filesystem:http://foo.com/test/dir a/dir b";
const char kChild[]  = "filesystem:http://foo.com/test/dir a/dir b/file";
const char kHasPeriod[]   = "filesystem:http://foo.com/test/dir a.dir b";

const char kOther1[] = "filesystem:http://foo.com/test/dir b";
const char kOther2[] = "filesystem:http://foo.com/temporary/dir a";

FileSystemURL URL(const char* spec) {
  return FileSystemURL::CreateForTest((GURL(spec)));
}

}  // namespace

class LocalFileSyncStatusTest : public testing::Test {
 public:
  LocalFileSyncStatusTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(LocalFileSyncStatusTest, WritingSimple) {
  LocalFileSyncStatus status;

  status.StartWriting(URL(kFile));
  status.StartWriting(URL(kFile));
  status.EndWriting(URL(kFile));

  EXPECT_TRUE(status.IsWriting(URL(kFile)));
  EXPECT_TRUE(status.IsWriting(URL(kParent)));
  EXPECT_TRUE(status.IsWriting(URL(kChild)));
  EXPECT_FALSE(status.IsWriting(URL(kOther1)));
  EXPECT_FALSE(status.IsWriting(URL(kOther2)));

  // Adding writers doesn't change the entry's writability.
  EXPECT_TRUE(status.IsWritable(URL(kFile)));
  EXPECT_TRUE(status.IsWritable(URL(kParent)));
  EXPECT_TRUE(status.IsWritable(URL(kChild)));
  EXPECT_TRUE(status.IsWritable(URL(kOther1)));
  EXPECT_TRUE(status.IsWritable(URL(kOther2)));

  // Adding writers makes the entry non-syncable.
  EXPECT_FALSE(status.IsSyncable(URL(kFile)));
  EXPECT_FALSE(status.IsSyncable(URL(kParent)));
  EXPECT_FALSE(status.IsSyncable(URL(kChild)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther1)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther2)));

  status.EndWriting(URL(kFile));

  EXPECT_FALSE(status.IsWriting(URL(kFile)));
  EXPECT_FALSE(status.IsWriting(URL(kParent)));
  EXPECT_FALSE(status.IsWriting(URL(kChild)));
}

TEST_F(LocalFileSyncStatusTest, SyncingSimple) {
  LocalFileSyncStatus status;

  status.StartSyncing(URL(kFile));

  EXPECT_FALSE(status.IsWritable(URL(kFile)));
  EXPECT_FALSE(status.IsWritable(URL(kParent)));
  EXPECT_FALSE(status.IsWritable(URL(kChild)));
  EXPECT_TRUE(status.IsWritable(URL(kOther1)));
  EXPECT_TRUE(status.IsWritable(URL(kOther2)));

  // New sync cannot be started for entries that are already in syncing.
  EXPECT_FALSE(status.IsSyncable(URL(kFile)));
  EXPECT_FALSE(status.IsSyncable(URL(kParent)));
  EXPECT_FALSE(status.IsSyncable(URL(kChild)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther1)));
  EXPECT_TRUE(status.IsSyncable(URL(kOther2)));

  status.EndSyncing(URL(kFile));

  EXPECT_TRUE(status.IsWritable(URL(kFile)));
  EXPECT_TRUE(status.IsWritable(URL(kParent)));
  EXPECT_TRUE(status.IsWritable(URL(kChild)));
}

TEST_F(LocalFileSyncStatusTest, WritingOnPathsWithPeriod) {
  LocalFileSyncStatus status;

  status.StartWriting(URL(kParent));
  status.StartWriting(URL(kHasPeriod));

  EXPECT_TRUE(status.IsChildOrParentWriting(URL(kFile)));

  status.EndWriting(URL(kParent));
  status.StartWriting(URL(kFile));

  EXPECT_TRUE(status.IsChildOrParentWriting(URL(kParent)));
}

TEST_F(LocalFileSyncStatusTest, SyncingOnPathsWithPeriod) {
  LocalFileSyncStatus status;

  status.StartSyncing(URL(kParent));
  status.StartSyncing(URL(kHasPeriod));

  EXPECT_TRUE(status.IsChildOrParentSyncing(URL(kFile)));

  status.EndSyncing(URL(kParent));
  status.StartSyncing(URL(kFile));

  EXPECT_TRUE(status.IsChildOrParentSyncing(URL(kParent)));
}

}  // namespace sync_file_system
