// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/local_extension_cache.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "crypto/secure_hash.h"
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
  LocalExtensionCacheTest() {}
  ~LocalExtensionCacheTest() override {}

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
    if (filename)
      *filename = file;
    CreateFile(file, size, timestamp);
  }

  void CreateFile(const base::FilePath& file,
                  size_t size,
                  const base::Time& timestamp) {
    std::string data(size, 0);
    EXPECT_EQ(base::WriteFile(file, data.data(), data.size()), int(size));
    EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));
  }

  std::string CreateSignedExtensionFile(const base::FilePath& dir,
                                        const std::string& id,
                                        const std::string& version,
                                        size_t size,
                                        const base::Time& timestamp,
                                        base::FilePath* filename) {
    std::string data(size, 0);

    std::unique_ptr<crypto::SecureHash> hash =
        crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    hash->Update(data.c_str(), size);
    uint8_t output[crypto::kSHA256Length];
    hash->Finish(output, sizeof(output));
    const std::string hex_hash =
        base::ToLowerASCII(base::HexEncode(output, sizeof(output)));

    const base::FilePath file =
        GetExtensionFileName(dir, id, version, hex_hash);
    if (filename)
      *filename = file;
    EXPECT_EQ(base::WriteFile(file, data.data(), data.size()), int(size));
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

 private:
  content::BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir cache_dir_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(LocalExtensionCacheTest);
};

static void SimpleCallback(bool* ptr) {
  *ptr = true;
}

TEST_F(LocalExtensionCacheTest, Basic) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::TimeDelta::FromDays(30),
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::Bind(&SimpleCallback, &initialized));

  base::FilePath file10, file01, file20, file30;
  CreateExtensionFile(cache_dir, kTestExtensionId1, "1.0", 100,
                      base::Time::Now() - base::TimeDelta::FromDays(1),
                      &file10);
  CreateExtensionFile(cache_dir, kTestExtensionId1, "0.1", 100,
                      base::Time::Now() - base::TimeDelta::FromDays(10),
                      &file01);
  CreateExtensionFile(cache_dir, kTestExtensionId2, "2.0", 100,
                      base::Time::Now() - base::TimeDelta::FromDays(40),
                      &file20);
  CreateExtensionFile(cache_dir, kTestExtensionId3, "3.0", 900,
                      base::Time::Now() - base::TimeDelta::FromDays(41),
                      &file30);

  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Older version should be removed on cache initialization.
  EXPECT_FALSE(base::PathExists(file01));

  // All extensions should be there because cleanup happens on shutdown to
  // support use case when device was not used to more than 30 days and cache
  // shouldn't be cleaned before someone will have a chance to use it.
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId2, "", NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId3, "", NULL, NULL));

  bool did_shutdown = false;
  cache.Shutdown(base::Bind(&SimpleCallback, &did_shutdown));
  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(did_shutdown);

  EXPECT_TRUE(base::PathExists(file10));
  EXPECT_FALSE(base::PathExists(file20));
  EXPECT_FALSE(base::PathExists(file30));
}

TEST_F(LocalExtensionCacheTest, KeepHashed) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::TimeDelta::FromDays(30),
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::Bind(&SimpleCallback, &initialized));

  // Add three identical extensions with different hash sums
  const base::Time time = base::Time::Now() - base::TimeDelta::FromDays(1);
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
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", NULL, &version));
  EXPECT_EQ(version, "1.0");
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash1, NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash2, NULL, NULL));
}

TEST_F(LocalExtensionCacheTest, KeepLatest) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::TimeDelta::FromDays(30),
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::Bind(&SimpleCallback, &initialized));

  // All extension files are hashed, but have different versions
  const base::Time time = base::Time::Now() - base::TimeDelta::FromDays(1);
  base::FilePath file1, file21, file22;
  const std::string hash1 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "1.0", 100, time, &file1);
  const std::string hash21 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "2.0", 101, time, &file21);
  const std::string hash22 = CreateSignedExtensionFile(
      cache_dir, kTestExtensionId1, "2.0", 123, time, &file22);

  content::RunAllTasksUntilIdle();
  ASSERT_TRUE(initialized);

  // Older version should be removed
  EXPECT_FALSE(base::PathExists(file1));
  // Both newer hashed versions should stay
  EXPECT_TRUE(base::PathExists(file21));
  EXPECT_TRUE(base::PathExists(file22));

  // We should be able to lookup only the latest version queries
  EXPECT_FALSE(cache.GetExtension(kTestExtensionId1, hash1, NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash21, NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash22, NULL, NULL));
}

TEST_F(LocalExtensionCacheTest, Complex) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::TimeDelta::FromDays(30),
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::Bind(&SimpleCallback, &initialized));

  // Like in KeepHashed test, but with two different versions
  const base::Time time = base::Time::Now() - base::TimeDelta::FromDays(1);
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

  // Older and unhashed versions should be removed
  EXPECT_FALSE(base::PathExists(file1));
  EXPECT_FALSE(base::PathExists(file11));
  EXPECT_FALSE(base::PathExists(file12));
  EXPECT_FALSE(base::PathExists(file2));
  // Newest hashed versions should stay
  EXPECT_TRUE(base::PathExists(file21));
  EXPECT_TRUE(base::PathExists(file22));

  // We should be able to lookup only the latest version queries, both with and
  // without hash
  std::string version;
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", NULL, &version));
  EXPECT_EQ(version, "2.0");
  EXPECT_FALSE(cache.GetExtension(kTestExtensionId1, hash11, NULL, NULL));
  EXPECT_FALSE(cache.GetExtension(kTestExtensionId1, hash12, NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash21, NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash22, NULL, NULL));
}

static void OnPutExtension(std::unique_ptr<base::RunLoop>* run_loop,
                           const base::FilePath& file_path,
                           bool file_ownership_passed) {
  ASSERT_TRUE(*run_loop);
  (*run_loop)->Quit();
}

static void PutExtensionAndWait(LocalExtensionCache& cache,
                                const std::string& id,
                                const std::string& expected_hash,
                                const base::FilePath& path,
                                const std::string& version) {
  std::unique_ptr<base::RunLoop> run_loop;
  run_loop.reset(new base::RunLoop);
  cache.PutExtension(id, expected_hash, path, version,
                     base::Bind(&OnPutExtension, &run_loop));
  run_loop->Run();
}

TEST_F(LocalExtensionCacheTest, PutExtensionCases) {
  base::FilePath cache_dir(CreateCacheDir());

  LocalExtensionCache cache(
      cache_dir, 1000, base::TimeDelta::FromDays(30),
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()}));
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::Bind(&SimpleCallback, &initialized));

  // Initialize cache with several different files
  const base::Time time = base::Time::Now() - base::TimeDelta::FromDays(1);
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

  // Right now we have two files for the first extension
  EXPECT_TRUE(base::PathExists(file11));
  EXPECT_TRUE(base::PathExists(file12));
  EXPECT_TRUE(base::PathExists(file2));
  EXPECT_TRUE(base::PathExists(file3));

  // 1. Cache contains an older version
  base::FilePath temp1;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 110, time, &temp1);
  PutExtensionAndWait(cache, kTestExtensionId1, "", temp1, "3.0");
  // New file added
  const base::FilePath unhashed =
      GetExtensionFileName(cache_dir, kTestExtensionId1, "3.0", "");
  EXPECT_TRUE(base::PathExists(unhashed));
  // Old files removed from cache (kept in the directory though)
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash11, NULL, &version));
  EXPECT_EQ(version, "3.0");
  EXPECT_TRUE(base::DeleteFile(temp1, false));

  // 2. Cache contains a newer version
  base::FilePath temp2;
  CreateExtensionFile(temp_path, kTestExtensionId1, "2.0", 120, time, &temp2);
  PutExtensionAndWait(cache, kTestExtensionId1, "", temp2, "2.0");
  // New file skipped
  EXPECT_FALSE(base::PathExists(
      GetExtensionFileName(cache_dir, kTestExtensionId1, "2.0", "")));
  // Old file kept
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", NULL, &version));
  EXPECT_EQ(version, "3.0");
  EXPECT_TRUE(base::DeleteFile(temp2, false));

  // 3. Cache contains the same version without hash, our file is unhashed
  base::FilePath temp3;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 130, time, &temp3);
  PutExtensionAndWait(cache, kTestExtensionId1, "", temp3, "3.0");
  // New file skipped, old file kept
  EXPECT_EQ(base::File(unhashed, base::File::FLAG_READ | base::File::FLAG_OPEN)
                .GetLength(),
            110);
  EXPECT_TRUE(base::DeleteFile(temp3, false));

  // 4. Cache contains the same version without hash, our file is hashed
  base::FilePath temp4;
  const std::string hash3 = CreateSignedExtensionFile(
      temp_path, kTestExtensionId1, "3.0", 140, time, &temp4);
  PutExtensionAndWait(cache, kTestExtensionId1, hash3, temp4, "3.0");
  // New file added
  const base::FilePath hashed =
      GetExtensionFileName(cache_dir, kTestExtensionId1, "3.0", hash3);
  EXPECT_TRUE(base::PathExists(hashed));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash3, NULL, NULL));
  // Old file removed (queries return hashed version)
  base::FilePath unhashed_path;
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, "", &unhashed_path, NULL));
  EXPECT_EQ(unhashed_path, hashed);
  EXPECT_TRUE(base::DeleteFile(temp4, false));
  EXPECT_TRUE(base::DeleteFile(unhashed, false));

  // 5. Cache contains the same version with hash, our file is unhashed
  base::FilePath temp5;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 150, time, &temp5);
  PutExtensionAndWait(cache, kTestExtensionId1, "", temp5, "3.0");
  // New file skipped
  EXPECT_FALSE(base::PathExists(unhashed));
  // Old file kept
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash3, NULL, NULL));
  EXPECT_TRUE(base::DeleteFile(temp5, false));

  // 6. Cache contains the same version with hash, our file has the "same" hash
  base::FilePath temp6;
  CreateExtensionFile(temp_path, kTestExtensionId1, "3.0", 160, time, &temp6);
  PutExtensionAndWait(cache, kTestExtensionId1, hash3, temp6, "3.0");
  // New file skipped, old file kept
  EXPECT_EQ(base::File(hashed, base::File::FLAG_READ | base::File::FLAG_OPEN)
                .GetLength(),
            140);
  EXPECT_TRUE(base::DeleteFile(temp6, false));

  // 7. Cache contains the same version with hash, our file is different
  base::FilePath temp7;
  const std::string hash4 = CreateSignedExtensionFile(
      temp_path, kTestExtensionId1, "3.0", 170, time, &temp7);
  PutExtensionAndWait(cache, kTestExtensionId1, hash4, temp7, "3.0");
  // New file addded
  const base::FilePath hashed2 =
      GetExtensionFileName(cache_dir, kTestExtensionId1, "3.0", hash4);
  EXPECT_TRUE(base::PathExists(hashed2));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash4, NULL, NULL));
  // Old file kept
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, hash3, NULL, NULL));
  EXPECT_TRUE(base::DeleteFile(temp7, false));
}

}  // namespace extensions
