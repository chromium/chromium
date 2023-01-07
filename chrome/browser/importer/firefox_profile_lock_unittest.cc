// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/firefox_profile_lock.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

class FirefoxProfileLockTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(FirefoxProfileLockTest, LockTest) {
  FirefoxProfileLock lock1(temp_dir_.GetPath());
  ASSERT_TRUE(lock1.HasAcquired());
  lock1.Unlock();
  ASSERT_FALSE(lock1.HasAcquired());
  lock1.Lock();
  ASSERT_TRUE(lock1.HasAcquired());
}

// Tests basic functionality and verifies that the lock file is deleted after
// use.
TEST_F(FirefoxProfileLockTest, ProfileLock) {
  base::FilePath test_path = temp_dir_.GetPath();
  base::FilePath lock_file_path =
      test_path.Append(FirefoxProfileLock::kLockFileName);

  EXPECT_FALSE(base::PathExists(lock_file_path));
  auto lock = std::make_unique<FirefoxProfileLock>(test_path);
  EXPECT_TRUE(lock->HasAcquired());
  EXPECT_TRUE(base::PathExists(lock_file_path));
  lock->Unlock();
  EXPECT_FALSE(lock->HasAcquired());

  // In the posix code, we don't delete the file when releasing the lock.
#if !BUILDFLAG(IS_POSIX)
  EXPECT_FALSE(base::PathExists(lock_file_path));
#endif  // !BUILDFLAG(IS_POSIX)
  lock->Lock();
  EXPECT_TRUE(lock->HasAcquired());
  EXPECT_TRUE(base::PathExists(lock_file_path));
  lock->Lock();
  EXPECT_TRUE(lock->HasAcquired());
  lock->Unlock();
  EXPECT_FALSE(lock->HasAcquired());
  // In the posix code, we don't delete the file when releasing the lock.
#if !BUILDFLAG(IS_POSIX)
  EXPECT_FALSE(base::PathExists(lock_file_path));
#endif  // !BUILDFLAG(IS_POSIX)
}

// If for some reason the lock file is left behind by the previous owner, we
// should still be able to lock it, at least in the Windows implementation.
TEST_F(FirefoxProfileLockTest, ProfileLockOrphaned) {
  base::FilePath test_path = temp_dir_.GetPath();
  base::FilePath lock_file_path =
      test_path.Append(FirefoxProfileLock::kLockFileName);

  // Create the orphaned lock file.
  FILE* lock_file = base::OpenFile(lock_file_path, "w");
  ASSERT_TRUE(lock_file);
  base::CloseFile(lock_file);
  EXPECT_TRUE(base::PathExists(lock_file_path));

  auto lock = std::make_unique<FirefoxProfileLock>(test_path);
  EXPECT_TRUE(lock->HasAcquired());
  lock->Unlock();
  EXPECT_FALSE(lock->HasAcquired());
}

// This is broken on POSIX since the same process is allowed to reacquire a
// lock.
#if !BUILDFLAG(IS_POSIX)
// Tests two locks contending for the same lock file.
TEST_F(FirefoxProfileLockTest, ProfileLockContention) {
  base::FilePath test_path = temp_dir_.GetPath();

  auto lock1 = std::make_unique<FirefoxProfileLock>(test_path);
  EXPECT_TRUE(lock1->HasAcquired());

  auto lock2 = std::make_unique<FirefoxProfileLock>(test_path);
  EXPECT_FALSE(lock2->HasAcquired());

  lock1->Unlock();
  EXPECT_FALSE(lock1->HasAcquired());

  lock2->Lock();
  EXPECT_TRUE(lock2->HasAcquired());
  lock2->Unlock();
  EXPECT_FALSE(lock2->HasAcquired());
}
#endif
