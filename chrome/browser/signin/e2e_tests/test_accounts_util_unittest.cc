// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/e2e_tests/test_accounts_util.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FilePath;

namespace signin {
namespace test {

class TestAccountsUtilTest : public testing::Test {};

FilePath WriteContentToTemporaryFile(const char* contents,
                                     unsigned int length) {
  FilePath tmp_file;
  CHECK(base::CreateTemporaryFile(&tmp_file));
  unsigned int bytes_written = base::WriteFile(tmp_file, contents, length);
  CHECK_EQ(bytes_written, length);
  return tmp_file;
}

TEST(TestAccountsUtilTest, ParsingJson) {
  const char contents[] =
      "{ \n"
      "  \"TEST_ACCOUNT_1\": {\n"
      "    \"win\": {\n"
      "      \"user\": \"user1\",\n"
      "      \"password\": \"pwd1\"\n"
      "    }\n"
      "  }\n"
      "}";
  FilePath tmp_file =
      WriteContentToTemporaryFile(contents, sizeof(contents) - 1);
  TestAccountsUtil util;
  util.Init(tmp_file);
}

TEST(TestAccountsUtilTest, GetAccountForPlatformSpecific) {
  const char contents[] =
      "{ \n"
      "  \"TEST_ACCOUNT_1\": {\n"
      "    \"win\": {\n"
      "      \"user\": \"user1\",\n"
      "      \"password\": \"pwd1\"\n"
      "    },\n"
      "    \"mac\": {\n"
      "      \"user\": \"user1\",\n"
      "      \"password\": \"pwd1\"\n"
      "    },\n"
      "    \"linux\": {\n"
      "      \"user\": \"user1\",\n"
      "      \"password\": \"pwd1\"\n"
      "    },\n"
      "    \"chromeos\": {\n"
      "      \"user\": \"user1\",\n"
      "      \"password\": \"pwd1\"\n"
      "    },\n"
      "    \"android\": {\n"
      "      \"user\": \"user1\",\n"
      "      \"password\": \"pwd1\"\n"
      "    }\n"
      "  }\n"
      "}";
  FilePath tmp_file =
      WriteContentToTemporaryFile(contents, sizeof(contents) - 1);
  TestAccountsUtil util;
  util.Init(tmp_file);
  TestAccount ta;
  bool ret = util.GetAccount("TEST_ACCOUNT_1", ta);
  ASSERT_TRUE(ret);
  ASSERT_EQ(ta.user, "user1");
  ASSERT_EQ(ta.password, "pwd1");
}

TEST(TestAccountsUtilTest, GetAccountForAllPlatform) {
  const char contents[] =
      "{ \n"
      "  \"TEST_ACCOUNT_1\": {\n"
      "    \"all_platform\": {\n"
      "      \"user\": \"user_allplatform\",\n"
      "      \"password\": \"pwd_allplatform\"\n"
      "    }\n"
      "  }\n"
      "}";
  FilePath tmp_file =
      WriteContentToTemporaryFile(contents, sizeof(contents) - 1);
  TestAccountsUtil util;
  util.Init(tmp_file);
  TestAccount ta;
  bool ret = util.GetAccount("TEST_ACCOUNT_1", ta);
  ASSERT_TRUE(ret);
  ASSERT_EQ(ta.user, "user_allplatform");
  ASSERT_EQ(ta.password, "pwd_allplatform");
}

}  // namespace test
}  // namespace signin
