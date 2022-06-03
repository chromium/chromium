// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/supports_user_data.h"


#include "base/memory/ptr_util.h"
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

  SupportsUserData* supports_user_data_;
  const void* key_;
};

TEST(SupportsUserDataTest, ClearWorksRecursively) {
  TestSupportsUserData supports_user_data;
  char key = 0;
  supports_user_data.SetUserData(
      &key, std::make_unique<UsesItself>(&supports_user_data, &key));
  // Destruction of supports_user_data runs the actual test.
}

struct TestData : public SupportsUserData::Data {};

TEST(SupportsUserDataTest, Movable) {
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

TEST(SupportsUserDataTest, ClearAllUserData) {
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

}  // namespace
}  // namespace base
