// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_storage.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestOrigin[] = "https://foo.test";
const char kTestOhttpKey[] = "FooOhttpKey";
const char kTestKeyCommitment[] = "FooKeyCommitment";

const char kTestOrigin2[] = "https://bar.test";
const char kTestOhttpKey2[] = "BarOhttpKey";

const char kTestOrigin3[] = "https://baz.test";
const char kTestOhttpKey3[] = "BazOhttpKey";

class KAnonymityServiceStorageTest : public testing::TestWithParam<bool> {
 public:
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("KAnonymityService"));
  }

  std::unique_ptr<KAnonymityServiceStorage> CreateStorage() {
    std::unique_ptr<KAnonymityServiceStorage> storage;
    if (StorageIsPersistent()) {
      storage = CreateKAnonymitySqlStorageForPath(db_path());
    } else {
      storage = std::make_unique<KAnonymityServiceMemoryStorage>();
    }

    base::RunLoop run_loop;
    storage->WaitUntilReady(base::BindLambdaForTesting(
        [&run_loop](KAnonymityServiceStorage::InitStatus status) {
          EXPECT_EQ(status, KAnonymityServiceStorage::InitStatus::kInitOk);
          run_loop.Quit();
        }));
    run_loop.Run();

    return storage;
  }

  bool StorageIsPersistent() { return GetParam(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_directory_;
};

TEST_P(KAnonymityServiceStorageTest, InitializeDatabase) {
  std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();
}

TEST_P(KAnonymityServiceStorageTest, MultipleWaitUntilReady) {
  std::unique_ptr<KAnonymityServiceStorage> storage;
  if (StorageIsPersistent()) {
    storage = CreateKAnonymitySqlStorageForPath(db_path());
  } else {
    storage = std::make_unique<KAnonymityServiceMemoryStorage>();
  }
  int seq_num = 0;
  base::RunLoop run_loop;
  storage->WaitUntilReady(base::BindLambdaForTesting(
      [&](KAnonymityServiceStorage::InitStatus status) {
        EXPECT_EQ(status, KAnonymityServiceStorage::InitStatus::kInitOk);
        EXPECT_EQ(1, ++seq_num);
      }));
  storage->WaitUntilReady(base::BindLambdaForTesting(
      [&](KAnonymityServiceStorage::InitStatus status) {
        EXPECT_EQ(status, KAnonymityServiceStorage::InitStatus::kInitOk);
        EXPECT_EQ(2, ++seq_num);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_P(KAnonymityServiceStorageTest, SaveAndLoadOHTTPKeys) {
  url::Origin test_origin = url::Origin::Create(GURL(kTestOrigin));
  std::string test_ohttp_key(kTestOhttpKey);
  base::Time expiration = base::Time::Now();

  url::Origin test_origin2 = url::Origin::Create(GURL(kTestOrigin2));
  std::string test_ohttp_key2(kTestOhttpKey2);
  base::Time expiration2 = expiration + base::Seconds(1);

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();

    EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin));
    EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin2));

    storage->UpdateOHTTPKeyFor(test_origin, {test_ohttp_key, expiration});
    storage->UpdateOHTTPKeyFor(test_origin2, {test_ohttp_key2, expiration2});

    std::optional<OHTTPKeyAndExpiration> result;
    result = storage->GetOHTTPKeyFor(test_origin);
    ASSERT_TRUE(result);
    EXPECT_EQ(test_ohttp_key, result->key);
    EXPECT_EQ(expiration, result->expiration);

    result = storage->GetOHTTPKeyFor(test_origin2);
    ASSERT_TRUE(result);
    EXPECT_EQ(test_ohttp_key2, result->key);
    EXPECT_EQ(expiration2, result->expiration);
  }

  task_environment().RunUntilIdle();

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();
    std::optional<OHTTPKeyAndExpiration> result;

    if (StorageIsPersistent()) {
      // Should be persisted after the storage is closed and re-opened.

      result = storage->GetOHTTPKeyFor(test_origin);
      ASSERT_TRUE(result);
      EXPECT_EQ(test_ohttp_key, result->key);
      EXPECT_EQ(expiration, result->expiration);

      result = storage->GetOHTTPKeyFor(test_origin2);
      ASSERT_TRUE(result);
      EXPECT_EQ(test_ohttp_key2, result->key);
      EXPECT_EQ(expiration2, result->expiration);
    } else {
      // Storage should have been cleared.
      EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin));
      EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin2));

      // Set them to the expected values.
      storage->UpdateOHTTPKeyFor(test_origin, {test_ohttp_key, expiration});
      storage->UpdateOHTTPKeyFor(test_origin2, {test_ohttp_key2, expiration2});
    }

    // Modify an existing key.
    expiration += base::Seconds(4);
    storage->UpdateOHTTPKeyFor(test_origin, {test_ohttp_key, expiration});

    result = storage->GetOHTTPKeyFor(test_origin);
    ASSERT_TRUE(result);
    EXPECT_EQ(test_ohttp_key, result->key);
    EXPECT_EQ(expiration, result->expiration);
  }

  task_environment().RunUntilIdle();

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();
    std::optional<OHTTPKeyAndExpiration> result;

    if (StorageIsPersistent()) {
      // Modifications should be persisted after the storage is closed and
      // re-opened.

      result = storage->GetOHTTPKeyFor(test_origin);
      ASSERT_TRUE(result);
      EXPECT_EQ(test_ohttp_key, result->key);
      EXPECT_EQ(expiration, result->expiration);

      result = storage->GetOHTTPKeyFor(test_origin2);
      ASSERT_TRUE(result);
      EXPECT_EQ(test_ohttp_key2, result->key);
      EXPECT_EQ(expiration2, result->expiration);
    } else {
      // Storage should have been cleared.
      EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin));
      EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin2));
    }
  }
}

TEST_P(KAnonymityServiceStorageTest, SaveAndLoadTooManyOHTTPKeys) {
  url::Origin test_origin = url::Origin::Create(GURL(kTestOrigin));
  std::string test_ohttp_key(kTestOhttpKey);
  base::Time expiration = base::Time::Now();

  url::Origin test_origin2 = url::Origin::Create(GURL(kTestOrigin2));
  std::string test_ohttp_key2(kTestOhttpKey2);
  base::Time expiration2 = expiration + base::Seconds(1);

  url::Origin test_origin3 = url::Origin::Create(GURL(kTestOrigin3));
  std::string test_ohttp_key3(kTestOhttpKey3);
  base::Time expiration3 = expiration2 + base::Seconds(2);

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();

    EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin));
    EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin2));
    EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin3));

    storage->UpdateOHTTPKeyFor(test_origin, {test_ohttp_key, expiration});
    storage->UpdateOHTTPKeyFor(test_origin2, {test_ohttp_key2, expiration2});
    storage->UpdateOHTTPKeyFor(test_origin3, {test_ohttp_key3, expiration3});

    std::optional<OHTTPKeyAndExpiration> result;
    result = storage->GetOHTTPKeyFor(test_origin);
    if (StorageIsPersistent()) {
      // The oldest should be forgotten.
      result = storage->GetOHTTPKeyFor(test_origin);
      ASSERT_FALSE(result);
    } else {
      ASSERT_TRUE(result);
      EXPECT_EQ(test_ohttp_key, result->key);
      EXPECT_EQ(expiration, result->expiration);
    }

    result = storage->GetOHTTPKeyFor(test_origin2);
    ASSERT_TRUE(result);
    EXPECT_EQ(test_ohttp_key2, result->key);
    EXPECT_EQ(expiration2, result->expiration);

    result = storage->GetOHTTPKeyFor(test_origin3);
    ASSERT_TRUE(result);
    EXPECT_EQ(test_ohttp_key3, result->key);
    EXPECT_EQ(expiration3, result->expiration);
  }

  task_environment().RunUntilIdle();

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();
    std::optional<OHTTPKeyAndExpiration> result;

    if (StorageIsPersistent()) {
      // Modifications should be persisted after the storage is closed and
      // re-opened.

      // The oldest should be forgotten.
      result = storage->GetOHTTPKeyFor(test_origin);
      ASSERT_FALSE(result);

      result = storage->GetOHTTPKeyFor(test_origin2);
      ASSERT_TRUE(result);
      EXPECT_EQ(test_ohttp_key2, result->key);
      EXPECT_EQ(expiration2, result->expiration);

      result = storage->GetOHTTPKeyFor(test_origin3);
      ASSERT_TRUE(result);
      EXPECT_EQ(test_ohttp_key3, result->key);
      EXPECT_EQ(expiration3, result->expiration);
    } else {
      // Storage should have been cleared.
      EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin));
      EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin2));
      EXPECT_FALSE(storage->GetOHTTPKeyFor(test_origin3));
    }
  }
}

TEST_P(KAnonymityServiceStorageTest, SaveAndLoadKeyCommitment) {
  std::string test_key_commitment(kTestKeyCommitment);
  int non_unique_user_id = 1;
  base::Time expiration = base::Time::Now();

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();

    EXPECT_FALSE(storage->GetKeyAndNonUniqueUserId());

    storage->UpdateKeyAndNonUniqueUserId(
        {{test_key_commitment, non_unique_user_id}, expiration});

    std::optional<KeyAndNonUniqueUserIdWithExpiration> result;
    result = storage->GetKeyAndNonUniqueUserId();
    ASSERT_TRUE(result);
    EXPECT_EQ(test_key_commitment, result->key_and_id.key_commitment);
    EXPECT_EQ(non_unique_user_id, result->key_and_id.non_unique_user_id);
    EXPECT_EQ(expiration, result->expiration);
  }

  task_environment().RunUntilIdle();

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();
    std::optional<KeyAndNonUniqueUserIdWithExpiration> result;
    if (StorageIsPersistent()) {
      // Should be persisted after the storage is closed and re-opened.

      result = storage->GetKeyAndNonUniqueUserId();
      ASSERT_TRUE(result);
      EXPECT_EQ(test_key_commitment, result->key_and_id.key_commitment);
      EXPECT_EQ(non_unique_user_id, result->key_and_id.non_unique_user_id);
      EXPECT_EQ(expiration, result->expiration);
    } else {
      // Should have been cleared.
      EXPECT_FALSE(storage->GetKeyAndNonUniqueUserId());
      storage->UpdateKeyAndNonUniqueUserId(
          {{test_key_commitment, non_unique_user_id}, expiration});
    }

    non_unique_user_id += 1;
    storage->UpdateKeyAndNonUniqueUserId(
        {{test_key_commitment, non_unique_user_id}, expiration});
    result = storage->GetKeyAndNonUniqueUserId();
    ASSERT_TRUE(result);
    EXPECT_EQ(test_key_commitment, result->key_and_id.key_commitment);
    EXPECT_EQ(non_unique_user_id, result->key_and_id.non_unique_user_id);
    EXPECT_EQ(expiration, result->expiration);
  }

  task_environment().RunUntilIdle();

  {
    std::unique_ptr<KAnonymityServiceStorage> storage = CreateStorage();
    std::optional<KeyAndNonUniqueUserIdWithExpiration> result;
    if (StorageIsPersistent()) {
      // Modifications should be persisted after the storage is closed and
      // re-opened.
      result = storage->GetKeyAndNonUniqueUserId();
      ASSERT_TRUE(result);
      EXPECT_EQ(test_key_commitment, result->key_and_id.key_commitment);
      EXPECT_EQ(non_unique_user_id, result->key_and_id.non_unique_user_id);
      EXPECT_EQ(expiration, result->expiration);
    } else {
      // Should have been cleared.
      EXPECT_FALSE(storage->GetKeyAndNonUniqueUserId());
    }
  }
}

TEST_P(KAnonymityServiceStorageTest, HandlesDestructionBeforeReady) {
  std::unique_ptr<KAnonymityServiceStorage> storage;
  if (StorageIsPersistent()) {
    storage = CreateKAnonymitySqlStorageForPath(db_path());
  } else {
    storage = std::make_unique<KAnonymityServiceMemoryStorage>();
  }
  storage->WaitUntilReady(base::BindLambdaForTesting(
      [&](KAnonymityServiceStorage::InitStatus status) {
        if (StorageIsPersistent()) {
          // Persistent storage doesn't return synchronously, so it should have
          // already been released.
          EXPECT_FALSE(storage);
        } else {
          // Memory storage calls synchronously so we haven't released the
          // storage.
          EXPECT_TRUE(storage);
        }
      }));
  storage.reset();
  task_environment().RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    KAnonymityServiceStorageTest,
    ::testing::Values(false, true));

}  // namespace
