// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/rate_limiting_key_manager.h"

#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace {

class RateLimitingKeyManagerTest : public testing::Test {
 public:
  void InitManagerWithStableInput(const std::string& stable_input) {
    manager_ = std::make_unique<RateLimitingKeyManager>(stable_input);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<RateLimitingKeyManager> manager_;
};

TEST_F(RateLimitingKeyManagerTest, GeneratesUniqueKeys) {
  InitManagerWithStableInput("stable_input");
  auto token1 = base::UnguessableToken::CreateForTesting(0, 1);
  auto token2 = base::UnguessableToken::CreateForTesting(0, 2);
  std::string key1 = manager_->GetCurrentRateLimitingKey(token1);
  std::string key2 = manager_->GetCurrentRateLimitingKey(token2);

  EXPECT_FALSE(key1.empty());
  EXPECT_FALSE(key2.empty());
  EXPECT_NE(key1, key2);
}

TEST_F(RateLimitingKeyManagerTest, ReturnsSameKeyForSameProfile) {
  InitManagerWithStableInput("stable_input");
  auto token = base::UnguessableToken::CreateForTesting(0, 1);
  std::string key1 = manager_->GetCurrentRateLimitingKey(token);
  std::string key1_again = manager_->GetCurrentRateLimitingKey(token);

  EXPECT_EQ(key1, key1_again);
}

TEST_F(RateLimitingKeyManagerTest, ReturnsSameKeyForSameProfileBeforeExpiry) {
  InitManagerWithStableInput("stable_input");
  auto token = base::UnguessableToken::CreateForTesting(0, 1);
  std::string key1 = manager_->GetCurrentRateLimitingKey(token);
  task_environment_.FastForwardBy(0.5 * RateLimitingKeyManager::kTimeToLive);
  std::string key1_again = manager_->GetCurrentRateLimitingKey(token);

  EXPECT_EQ(key1, key1_again);
}

TEST_F(RateLimitingKeyManagerTest, ReturnsNewKeysForSameProfileAfterExpiry) {
  InitManagerWithStableInput("stable_input");
  auto token = base::UnguessableToken::CreateForTesting(0, 1);
  std::string key1 = manager_->GetCurrentRateLimitingKey(token);
  task_environment_.FastForwardBy(1.1 * RateLimitingKeyManager::kTimeToLive);
  std::string key1_new = manager_->GetCurrentRateLimitingKey(token);
  task_environment_.FastForwardBy(1.1 * RateLimitingKeyManager::kTimeToLive);
  std::string key1_newer = manager_->GetCurrentRateLimitingKey(token);

  EXPECT_NE(key1, key1_new);
  EXPECT_NE(key1, key1_newer);
  EXPECT_NE(key1_new, key1_newer);
}

TEST_F(RateLimitingKeyManagerTest, DifferentInstancesGenerateUniqueKeys) {
  auto token = base::UnguessableToken::CreateForTesting(0, 1);
  InitManagerWithStableInput("stable_input");
  std::string manager1_key1 = manager_->GetCurrentRateLimitingKey(token);

  // Instantiating a different manager, even if using the same stable_input
  // value, should result in newly generated keys.
  InitManagerWithStableInput("stable_input");
  std::string manager2_key1 = manager_->GetCurrentRateLimitingKey(token);

  // A different instance with a different stable_input should also generate
  // distinct keys.
  InitManagerWithStableInput("different_stable_input");
  std::string manager3_key1 = manager_->GetCurrentRateLimitingKey(token);

  EXPECT_NE(manager1_key1, manager2_key1);
  EXPECT_NE(manager2_key1, manager3_key1);
  EXPECT_NE(manager1_key1, manager3_key1);
}

}  // namespace
}  // namespace safe_browsing
