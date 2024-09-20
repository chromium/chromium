// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/local_extension_cache.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtensionId3[] = "cccccccccccccccccccccccccccccccc";

}  // namespace

namespace extensions {

class LocalExtensionCacheTest : public testing::Test {
 public:
  LocalExtensionCacheTest() = default;

  LocalExtensionCacheTest(const LocalExtensionCacheTest&) = delete;
  LocalExtensionCacheTest& operator=(const LocalExtensionCacheTest&) = delete;

  ~LocalExtensionCacheTest() override = default;

  base::FilePath CreateCacheDir() {
    EXPECT_TRUE(cache_dir_.CreateUniqueTempDir());
    return cache_dir_.GetPath();
  }

  base::FilePath CreateTempDir() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    return temp_dir_.GetPath();
  }

  void CreateExtensionFile(const base::FilePath& dir,
                           const std::string& id,
                           const std::string& version,
                           size_t size,
                           const base::Time& timestamp,
                           base::FilePath* filename) {
    const base::FilePath file = GetExtensionFileName(dir, id, version, "");
    if (filename) {
      *filename = file;
    }
    CreateFile(file, size, timestamp);
  }

  void CreateFile(const base::FilePath& file,
                  size_t size,
                  const base::Time& timestamp) {
    std::string data(size, 0);
    EXPECT_TRUE(base::WriteFile(file, data));
    EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));
  }

  std::string CreateSignedExtensionFile(const base::FilePath& dir,
                                        const std::string& id,
                                        const std::string& version,
                                        size_t size,
                                        const base::Time& timestamp,
                                        base::FilePath* filename) {
    std::string data(size, 0);
    const std::string hex_hash = base::ToLowerASCII(
        base::HexEncode(crypto::SHA256Hash(base::as_byte_span(data))));

    const base::FilePath file =
        GetExtensionFileName(dir, id, version, hex_hash);
    if (filename) {
      *filename = file;
    }
    EXPECT_TRUE(base::WriteFile(file, data));
    EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));

    return hex_hash;
  }

  base::FilePath GetExtensionFileName(const base::FilePath& dir,
                                      const std::string& id,
                                      const std::string& version,
                                      const std::string& hash) {
    return dir.Append(
        extensions::LocalExtensionCache::ExtensionFileName(id, version, hash));
  }

  base::FilePath GetInvalidCacheFilePath() {
    return cache_dir_.GetPath().AppendASCII(
        LocalExtensionCache::kInvalidCacheIdsFileName);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir cache_dir_;
  base::ScopedTempDir temp_dir_;
};

static void SimpleCallback(bool* ptr) {
  *ptr = true;
}

TEST_F(LocalExtensionCacheTest, Basic) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::Days(30),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::BindOnce(&SimpleCallback, &initialized));

  base::FilePath file10, file01, file20, file30;
  CreateExtensionFile(cache_dir, kTestExtensionId1, "1.0", 100,
                      base::Time::Now() - base::Days(1), &file10);
  CreateExtensionFile(cache_dir, kTestExtensionId1, "0.1", 100,
                      base::Time::Now() - base::Days(10), &file01);
  CreateExtensionFile(cache_dir, kTestExtensionId2, "2.0", 100,
                      base::Time::Now() - base::Days(40), &file20);
  CreateExtensionFile(cache_dir, kTestExtensionId3, "3.0", 900,
                      base::Time::Now() - base::Days(41), &file30);

  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Older version should be removed on cache initialization.
  EXPECT_FALSE(base::PathExists(file01));

  // All extensions should be there because cleanup happens on shutdown to
  // support use case when device was not used to more than 30 days and cache
  // shouldn't be cleaned before someone will have a chance to use it.
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId2, "", nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId3, "", nullptr, nullptr));

  bool did_shutdown = false;
  cache.Shutdown(base::BindOnce(&SimpleCallback, &did_shutdown));
  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(did_shutdown);

  EXPECT_TRUE(base::PathExists(file10));
  EXPECT_FALSE(base::PathExists(file20));
  EXPECT_FALSE(base::PathExists(file30));
}

TEST_F(LocalExtensionCacheTest, KeepHashed) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::Days(30),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::BindOnce(&SimpleCallback, &initialized));

  // Add three identical extensions with different hash sums.
  const base::Time time = base::Time::Now() - base::Days(1);
  base::FilePath file, file1, file2;
  CreateExtensionFile(cache_dir, kTestExtensionId1, "1.0", 100, time, &file);
  const std::string hash1 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 100, time, &file1);
  const std::string hash2 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 123, time, &file2);

  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Unhashed version should be removed on cache initialization.
  EXPECT_FALSE(base::PathExists(file));
  // Both hashed versions should stay
  EXPECT_TRUE(base::PathExists(file1));
  EXPECT_TRUE(base::PathExists(file2));

  // We should be able to lookup all three extension queries
  std::string version;
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", nullptr, &version));
  EXPECT_EQ(version, "1.0");
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash1, nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash2, nullptr, nullptr));
}

TEST_F(LocalExtensionCacheTest, KeepLatest) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::Days(30),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::BindOnce(&SimpleCallback, &initialized));

  // All extension files are hashed, but have different versions.
  const base::Time time = base::Time::Now() - base::Days(1);
  base::FilePath file1, file21, file22;
  const std::string hash1 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 100, time, &file1);
  const std::string hash21 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "2.0", 101, time, &file21);
  const std::string hash22 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "2.0", 123, time, &file22);

  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Older version should be removed.
  EXPECT_FALSE(base::PathExists(file1));
  // Both newer hashed versions should stay.
  EXPECT_TRUE(base::PathExists(file21));
  EXPECT_TRUE(base::PathExists(file22));

  // We should be able to lookup only the latest version queries.
  EXPECT_FALSE(cache.GetExtension(kTestExtensionId1, hash1, nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash21, nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash22, nullptr, nullptr));
}

TEST_F(LocalExtensionCacheTest, Complex) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::Days(30),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::BindOnce(&SimpleCallback, &initialized));

  // Like in KeepHashed test, but with two different versions.
  const base::Time time = base::Time::Now() - base::Days(1);
  base::FilePath file1, file11, file12, file2, file21, file22;
  CreateExtensionFile(cache_dir, kTestExtensionId1, "1.0", 100, time, &file1);
  const std::string hash11 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 101, time, &file11);
  const std::string hash12 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 102, time, &file12);
  CreateExtensionFile(cache_dir, kTestExtensionId1, "2.0", 103, time, &file2);
  const std::string hash21 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "2.0", 104, time, &file21);
  const std::string hash22 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "2.0", 105, time, &file22);

  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Older and unhashed versions should be removed.
  EXPECT_FALSE(base::PathExists(file1));
  EXPECT_FALSE(base::PathExists(file11));
  EXPECT_FALSE(base::PathExists(file12));
  EXPECT_FALSE(base::PathExists(file2));
  // Newest hashed versions should stay.
  EXPECT_TRUE(base::PathExists(file21));
  EXPECT_TRUE(base::PathExists(file22));

  // We should be able to lookup only the latest version queries, both with and
  // without hash.
  std::string version;
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", nullptr, &version));
  EXPECT_EQ(version, "2.0");
  EXPECT_FALSE(cache.GetExtension(kTestExtensionId1, hash11, nullptr, nullptr));
  EXPECT_FALSE(cache.GetExtension(kTestExtensionId1, hash12, nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash21, nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash22, nullptr, nullptr));
}

static void PutExtensionAndWait(LocalExtensionCache* cache,
                                const std::string& id,
                                const std::string& expected_hash,
                                const base::FilePath& path,
                                const std::string& version) {
  base::RunLoop run_loop;
  cache->PutExtension(
      id, expected_hash, path, base::Version(version),
      base::BindRepeating([](base::RunLoop* run_loop, const base::FilePath&,
                             bool) { run_loop->Quit(); },
                          &run_loop));
  run_loop.Run();
}

TEST_F(LocalExtensionCacheTest, PutExtensionCases) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::Days(30),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::BindOnce(&SimpleCallback, &initialized));

  // Initialize cache with several different files
  const base::Time time = base::Time::Now() - base::Days(1);
  base::FilePath file11, file12, file2, file3;
  const std::string hash11 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 101, time, &file11);
  const std::string hash12 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 102, time, &file12);
  CreateSignedExtensionFile(cache_dir, kTestExtensionId2, "0.2", 200, time,
                            &file2);
  CreateExtensionFile(cache_dir, kTestExtensionId3, "0.3", 300, time, &file3);

  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Create and initialize installation source directory.
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath temp_path = temp_dir.GetPath();
  std::string version;

  // Right now we have two files for the first extension.
  EXPECT_TRUE(base::PathExists(file11));
  EXPECT_TRUE(base::PathExists(file12));
  EXPECT_TRUE(base::PathExists(file2));
  EXPECT_TRUE(base::PathExists(file3));

  // 1. Cache contains an older version.
  base::FilePath temp1;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 110, time, &temp1);
  PutExtensionAndWait(&cache, kTestExtensionId1, "", temp1, "3.0");
  // New file added.
  const base::FilePath unhashed =
      GetExtensionFileName(cache_dir, kTestExtensionId1, "3.0", "");
  EXPECT_TRUE(base::PathExists(unhashed));
  // Old files removed from cache (kept in the directory though).
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash11, nullptr, &version));
  EXPECT_EQ(version, "3.0");
  EXPECT_TRUE(base::DeleteFile(temp1));

  // 2. Cache contains a newer version.
  base::FilePath temp2;
  CreateExtensionFile(temp_path, kTestExtensionId1, "2.0", 120, time, &temp2);
  PutExtensionAndWait(&cache, kTestExtensionId1, "", temp2, "2.0");
  // New file skipped.
  EXPECT_FALSE(base::PathExists(
      GetExtensionFileName(cache_dir, kTestExtensionId1, "2.0", "")));
  // Old file kept.
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", nullptr, &version));
  EXPECT_EQ(version, "3.0");
  EXPECT_TRUE(base::DeleteFile(temp2));

  // 3. Cache contains the same version without hash, our file is unhashed.
  base::FilePath temp3;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 130, time, &temp3);
  PutExtensionAndWait(&cache, kTestExtensionId1, "", temp3, "3.0");
  // New file skipped, old file kept
  EXPECT_EQ(base::File(unhashed, base::File::FLAG_READ | base::File::FLAG_OPEN)
                .GetLength(),
            110);
  EXPECT_TRUE(base::DeleteFile(temp3));

  // 4. Cache contains the same version without hash, our file is hashed.
  base::FilePath temp4;
  const std::string hash3 = CreateSignedExtensionFile(
      temp_path, kTestExtensionId1, "3.0", 140, time, &temp4);
  PutExtensionAndWait(&cache, kTestExtensionId1, hash3, temp4, "3.0");
  // New file added.
  const base::FilePath hashed =
      GetExtensionFileName(cache_dir, kTestExtensionId1, "3.0", hash3);
  EXPECT_TRUE(base::PathExists(hashed));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash3, nullptr, nullptr));
  // Old file removed (queries return hashed version)
  base::FilePath unhashed_path;
  EXPECT_TRUE(
      cache.GetExtension(kTestExtensionId1, "", &unhashed_path, nullptr));
  EXPECT_EQ(unhashed_path, hashed);
  EXPECT_TRUE(base::DeleteFile(temp4));
  EXPECT_TRUE(base::DeleteFile(unhashed));

  // 5. Cache contains the same version with hash, our file is unhashed.
  base::FilePath temp5;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 150, time, &temp5);
  PutExtensionAndWait(&cache, kTestExtensionId1, "", temp5, "3.0");
  // New file skipped.
  EXPECT_FALSE(base::PathExists(unhashed));
  // Old file kept.
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash3, nullptr, nullptr));
  EXPECT_TRUE(base::DeleteFile(temp5));

  // 6. Cache contains the same version with hash, our file has the "same" hash.
  base::FilePath temp6;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 160, time, &temp6);
  PutExtensionAndWait(&cache, kTestExtensionId1, hash3, temp6, "3.0");
  // New file skipped, old file kept
  EXPECT_EQ(base::File(hashed, base::File::FLAG_READ | base::File::FLAG_OPEN)
                .GetLength(),
            140);
  EXPECT_TRUE(base::DeleteFile(temp6));

  // 7. Cache contains the same version with hash, our file is different.
  base::FilePath temp7;
  const std::string hash4 = CreateSignedExtensionFile(
      temp_path, kTestExtensionId1, "3.0", 170, time, &temp7);
  PutExtensionAndWait(&cache, kTestExtensionId1, hash4, temp7, "3.0");
  // New file added.
  const base::FilePath hashed2 =
      GetExtensionFileName(cache_dir, kTestExtensionId1, "3.0", hash4);
  EXPECT_TRUE(base::PathExists(hashed2));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash4, nullptr, nullptr));
  // Old file kept.
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash3, nullptr, nullptr));
  EXPECT_TRUE(base::DeleteFile(temp7));
}

// This test checks that scheduling extension cache removal with
// `RemoveOnNextInit` works correctly: extension cache is deleted for the
// specified extension right on the next initialization, another extension is
// not affected.
TEST_F(LocalExtensionCacheTest, InvalidExtensionRemoval) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, /*max_cache_size=*/1000, /*max_cache_age=*/base::Days(30),
      /*backend_task_runner=*/
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::BindOnce(&SimpleCallback, &initialized));

  const base::Time time = base::Time::Now() - base::Days(1);
  base::FilePath file_1, file_2_1, file_2_2;
  const std::string hash_1 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 100, time, &file_1);
  const std::string hash_2_1 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId2, "2.0", 101, time, &file_2_1);
  const std::string hash_2_2 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId2, "2.0", 123, time, &file_2_2);
  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  EXPECT_TRUE(base::PathExists(file_1));
  EXPECT_TRUE(base::PathExists(file_2_1));
  EXPECT_TRUE(base::PathExists(file_2_2));

  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash_1, nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId2, "", nullptr, nullptr));

  // Invalid cache file should be removed on initializion.
  EXPECT_FALSE(base::PathExists(GetInvalidCacheFilePath()));

  cache.RemoveOnNextInit(kTestExtensionId2);
  content::RunAllTasksUntilIdle();

  // Extension files should still exist, nothing should be deleted before the
  // next initialization.
  EXPECT_TRUE(base::PathExists(file_1));
  EXPECT_TRUE(base::PathExists(file_2_1));
  EXPECT_TRUE(base::PathExists(file_2_2));

  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash_1, nullptr, nullptr));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId2, "", nullptr, nullptr));

  bool did_shutdown = false;
  cache.Shutdown(base::BindOnce(&SimpleCallback, &did_shutdown));
  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(did_shutdown);

  EXPECT_TRUE(base::PathExists(file_1));
  EXPECT_TRUE(base::PathExists(file_2_1));
  EXPECT_TRUE(base::PathExists(file_2_2));

  // Create cache again for the same directory.
  LocalExtensionCache new_cache(
      cache_dir, 1000, base::Days(30),
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  initialized = false;
  new_cache.Init(true, base::BindOnce(&SimpleCallback, &initialized));
  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Check that second extension's cache was cleaned up after initialization.
  EXPECT_TRUE(base::PathExists(file_1));
  EXPECT_FALSE(base::PathExists(file_2_1));
  EXPECT_FALSE(base::PathExists(file_2_2));

  EXPECT_TRUE(
      new_cache.GetExtension(kTestExtensionId1, hash_1, nullptr, nullptr));
  EXPECT_FALSE(new_cache.GetExtension(kTestExtensionId2, "", nullptr, nullptr));

  // Invalid cache file should be removed on initializion.
  EXPECT_FALSE(base::PathExists(GetInvalidCacheFilePath()));

  did_shutdown = false;
  new_cache.Shutdown(base::BindOnce(&SimpleCallback, &did_shutdown));
  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(did_shutdown);
}

}  // namespace extensions
