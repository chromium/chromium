// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/internal_auth.h"

#include <stddef.h>

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

class InternalAuthTest : public ::testing::Test {
 public:
  InternalAuthTest() {
    long_string_ = "seed";
    for (int i = 20; i--;)
      long_string_ += long_string_;
  }
  ~InternalAuthTest() override {}

  void SetUp() override {}

  void TearDown() override {}

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::string long_string_;
};

TEST_F(InternalAuthTest, BasicGeneration) {
  std::map<std::string, std::string> map;
  map["key"] = "value";
  std::string token = InternalAuthGeneration::GeneratePassport(
      "zapata", map);
  ASSERT_GT(token.size(), 10u);  // short token is insecure.

  map["key2"] = "value2";
  token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_GT(token.size(), 10u);
}

TEST_F(InternalAuthTest, DoubleGeneration) {
  std::map<std::string, std::string> map;
  map["key"] = "value";
  std::string token1 = InternalAuthGeneration::GeneratePassport(
      "zapata", map);
  ASSERT_GT(token1.size(), 10u);

  std::string token2 = InternalAuthGeneration::GeneratePassport(
      "zapata", map);
  ASSERT_GT(token2.size(), 10u);
  // tokens are different even if credentials coincide.
  ASSERT_NE(token1, token2);
}

TEST_F(InternalAuthTest, BadGeneration) {
  std::map<std::string, std::string> map;
  map["key"] = "value";
  // Trying huge domain.
  std::string token = InternalAuthGeneration::GeneratePassport(
      long_string_, map);
  ASSERT_TRUE(token.empty());
  ASSERT_FALSE(InternalAuthVerification::VerifyPassport(
      token, long_string_, map));

  // Trying empty domain.
  token = InternalAuthGeneration::GeneratePassport(std::string(), map);
  ASSERT_TRUE(token.empty());
  ASSERT_FALSE(
      InternalAuthVerification::VerifyPassport(token, std::string(), map));

  std::string dummy("abcdefghij");
  for (size_t i = 1000; i--;) {
    std::string key = dummy;
    std::next_permutation(dummy.begin(), dummy.end());
    std::string value = dummy;
    std::next_permutation(dummy.begin(), dummy.end());
    map[key] = value;
  }
  // Trying huge var=value map.
  token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_TRUE(token.empty());
  ASSERT_FALSE(InternalAuthVerification::VerifyPassport(token, "zapata", map));

  map.clear();
  map[std::string()] = "value";
  // Trying empty key.
  token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_TRUE(token.empty());
  ASSERT_FALSE(InternalAuthVerification::VerifyPassport(token, "zapata", map));
}

TEST_F(InternalAuthTest, BasicVerification) {
  std::map<std::string, std::string> map;
  map["key"] = "value";
  std::string token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_GT(token.size(), 10u);
  ASSERT_TRUE(InternalAuthVerification::VerifyPassport(token, "zapata", map));
  // Passport can not be reused.
  for (int i = 1000; i--;) {
    ASSERT_FALSE(InternalAuthVerification::VerifyPassport(
        token, "zapata", map));
  }
}

TEST_F(InternalAuthTest, BruteForce) {
  std::map<std::string, std::string> map;
  map["key"] = "value";
  std::string token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_GT(token.size(), 10u);

  // Trying bruteforce.
  std::string dummy = token;
  for (size_t i = 100; i--;) {
    std::next_permutation(dummy.begin(), dummy.end());
    ASSERT_FALSE(InternalAuthVerification::VerifyPassport(
        dummy, "zapata", map));
  }
  dummy = token;
  for (size_t i = 100; i--;) {
    std::next_permutation(dummy.begin(), dummy.begin() + dummy.size() / 2);
    ASSERT_FALSE(InternalAuthVerification::VerifyPassport(
        dummy, "zapata", map));
  }
  // We brute forced just too little, so original token must not expire yet.
  ASSERT_TRUE(InternalAuthVerification::VerifyPassport(token, "zapata", map));
}

TEST_F(InternalAuthTest, ExpirationAndBruteForce) {
  int kCustomVerificationWindow = 2;
  InternalAuthVerification::set_verification_window_seconds(
      kCustomVerificationWindow);

  std::map<std::string, std::string> map;
  map["key"] = "value";
  std::string token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_GT(token.size(), 10u);

  // We want to test token expiration, so we need to wait some amount of time,
  // so we are brute-forcing during this time.
  base::Time timestamp = base::Time::Now();
  std::string dummy1 = token;
  std::string dummy2 = token;
  for (;;) {
    for (size_t i = 100; i--;) {
      std::next_permutation(dummy1.begin(), dummy1.end());
      ASSERT_FALSE(InternalAuthVerification::VerifyPassport(
          dummy1, "zapata", map));
    }
    for (size_t i = 100; i--;) {
      std::next_permutation(dummy2.begin(), dummy2.begin() + dummy2.size() / 2);
      ASSERT_FALSE(InternalAuthVerification::VerifyPassport(
          dummy2, "zapata", map));
    }
    if (base::Time::Now() - timestamp > base::TimeDelta::FromSeconds(
        kCustomVerificationWindow + 1)) {
      break;
    }
  }
  ASSERT_FALSE(InternalAuthVerification::VerifyPassport(token, "zapata", map));
  // Reset verification window to default.
  InternalAuthVerification::set_verification_window_seconds(0);
}

TEST_F(InternalAuthTest, ChangeKey) {
  std::map<std::string, std::string> map;
  map["key"] = "value";
  std::string token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_GT(token.size(), 10u);

  InternalAuthGeneration::GenerateNewKey();
  // Passport should survive key change.
  ASSERT_TRUE(InternalAuthVerification::VerifyPassport(token, "zapata", map));

  token = InternalAuthGeneration::GeneratePassport("zapata", map);
  ASSERT_GT(token.size(), 10u);
  for (int i = 20; i--;)
    InternalAuthGeneration::GenerateNewKey();
  // Passport should not survive series of key changes.
  ASSERT_FALSE(InternalAuthVerification::VerifyPassport(token, "zapata", map));
}
