// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/supports_user_data.h"

#include "base/features.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

struct TestSupportsUserData : public SupportsUserData {
  // Make ClearAllUserData public so tests can access it.
  using SupportsUserData::ClearAllUserData;
};

struct UsesItself : public SupportsUserData::Data {
  UsesItself(SupportsUserData* supports_user_data, const void* key)
      : supports_user_data_(supports_user_data),
        key_(key) {
  }

  ~UsesItself() override {
    EXPECT_EQ(nullptr, supports_user_data_->GetUserData(key_));
  }

  raw_ptr<SupportsUserData> supports_user_data_;
  raw_ptr<const void> key_;
};

class SupportsUserDataTest : public ::testing::TestWithParam<bool> {
 public:
  SupportsUserDataTest() {
    if (GetParam()) {
      scoped_features_.InitWithFeatures(
          {features::kSupportsUserDataFlatHashMap}, {});
    } else {
      scoped_features_.InitWithFeatures(
          {}, {features::kSupportsUserDataFlatHashMap});
    }
  }

  base::test::ScopedFeatureList scoped_features_;
};

TEST_P(SupportsUserDataTest, ClearWorksRecursively) {
  char key = 0;  // Must outlive `supports_user_data`.
  TestSupportsUserData supports_user_data;
  supports_user_data.SetUserData(
      &key, std::make_unique<UsesItself>(&supports_user_data, &key));
  // Destruction of supports_user_data runs the actual test.
}

struct TestData : public SupportsUserData::Data {};

TEST_P(SupportsUserDataTest, Movable) {
  TestSupportsUserData supports_user_data_1;
  char key1 = 0;
  supports_user_data_1.SetUserData(&key1, std::make_unique<TestData>());
  void* data_1_ptr = supports_user_data_1.GetUserData(&key1);

  TestSupportsUserData supports_user_data_2;
  char key2 = 0;
  supports_user_data_2.SetUserData(&key2, std::make_unique<TestData>());

  supports_user_data_2 = std::move(supports_user_data_1);

  EXPECT_EQ(data_1_ptr, supports_user_data_2.GetUserData(&key1));
  EXPECT_EQ(nullptr, supports_user_data_2.GetUserData(&key2));
}

TEST_P(SupportsUserDataTest, ClearAllUserData) {
  TestSupportsUserData supports_user_data;
  char key1 = 0;
  supports_user_data.SetUserData(&key1, std::make_unique<TestData>());
  char key2 = 0;
  supports_user_data.SetUserData(&key2, std::make_unique<TestData>());

  EXPECT_TRUE(supports_user_data.GetUserData(&key1));
  EXPECT_TRUE(supports_user_data.GetUserData(&key2));

  supports_user_data.ClearAllUserData();

  EXPECT_FALSE(supports_user_data.GetUserData(&key1));
  EXPECT_FALSE(supports_user_data.GetUserData(&key2));
}

TEST_P(SupportsUserDataTest, TakeUserData) {
  TestSupportsUserData supports_user_data;
  char key1 = 0;
  supports_user_data.SetUserData(&key1, std::make_unique<TestData>());

  TestSupportsUserData::Data* data1_ptr = supports_user_data.GetUserData(&key1);
  EXPECT_NE(data1_ptr, nullptr);

  char wrong_key = 0;
  EXPECT_FALSE(supports_user_data.TakeUserData(&wrong_key));

  EXPECT_EQ(supports_user_data.GetUserData(&key1), data1_ptr);

  std::unique_ptr<TestSupportsUserData::Data> data1 =
      supports_user_data.TakeUserData(&key1);
  EXPECT_EQ(data1.get(), data1_ptr);

  EXPECT_FALSE(supports_user_data.GetUserData(&key1));
  EXPECT_FALSE(supports_user_data.TakeUserData(&key1));
}

class DataOwnsSupportsUserData : public SupportsUserData::Data {
 public:
  TestSupportsUserData* supports_user_data() { return &supports_user_data_; }

 private:
  TestSupportsUserData supports_user_data_;
};

// Tests that removing a `SupportsUserData::Data` that owns a `SupportsUserData`
// does not crash.
TEST_P(SupportsUserDataTest, ReentrantRemoveUserData) {
  DataOwnsSupportsUserData* data = new DataOwnsSupportsUserData;
  char key = 0;
  data->supports_user_data()->SetUserData(&key, WrapUnique(data));
  data->supports_user_data()->RemoveUserData(&key);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SupportsUserDataTest,
                         testing::Values(false, true));

}  // namespace
}  // namespace base
