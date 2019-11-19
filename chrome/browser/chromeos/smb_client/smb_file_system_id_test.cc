// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_file_system_id.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::MatchesRegex;

namespace chromeos {
namespace smb_client {

namespace {
// gmock "regex" support is very basic and doesn't support [] or + operations.
// So write out the \w 16 times. This will incorrectly match _, but we can live
// with that.
constexpr char kRandomIdRegex[] =
    "^\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w\\w";
}  // namespace

constexpr char kTestUsername[] = "foobar";
constexpr char kTestWorkgroup[] = "accounts.baz.com";

class SmbFileSystemIdTest : public testing::Test {
 public:
  SmbFileSystemIdTest() = default;
  ~SmbFileSystemIdTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SmbFileSystemIdTest);
};

TEST_F(SmbFileSystemIdTest, ShouldCreateFileSystemIdCorrectly) {
  const base::FilePath share_path("smb://192.168.0.0/test");

  EXPECT_THAT(
      CreateFileSystemId(share_path, false /* is_kerberos */),
      MatchesRegex(std::string(kRandomIdRegex) + "@@smb://192.168.0.0/test"));
  EXPECT_THAT(CreateFileSystemId(share_path, true /* is_kerberos */),
              MatchesRegex(std::string(kRandomIdRegex) +
                           "@@smb://192.168.0.0/test@@kerberos_chromad"));
}

TEST_F(SmbFileSystemIdTest, CreateFileSystemIdForUser) {
  const base::FilePath share_path("smb://192.168.0.0/test");

  EXPECT_THAT(CreateFileSystemIdForUser(share_path, kTestUsername),
              MatchesRegex(std::string(kRandomIdRegex) +
                           "@@smb://192.168.0.0/test@@user=" + kTestUsername));

  std::string user_workgroup =
      base::StrCat({kTestUsername, "@", kTestWorkgroup});
  EXPECT_THAT(CreateFileSystemIdForUser(share_path, user_workgroup),
              MatchesRegex(std::string(kRandomIdRegex) +
                           "@@smb://192.168.0.0/test@@user=" + user_workgroup));
}

TEST_F(SmbFileSystemIdTest, ShouldParseSharePathCorrectly) {
  // Note: These are still valid because existing users might have saved shares.
  const std::string file_system_id_1 = "12@@smb://192.168.0.0/test";
  const base::FilePath expected_share_path_1("smb://192.168.0.0/test");
  const std::string file_system_id_2 =
      "13@@smb://192.168.0.1/test@@kerberos_chromad";
  const base::FilePath expected_share_path_2("smb://192.168.0.1/test");

  const std::string file_system_id_3 =
      "EFAFF3864D0FE389@@smb://192.168.0.1/test@@kerberos_chromad";
  const base::FilePath expected_share_path_3("smb://192.168.0.1/test");

  EXPECT_EQ(expected_share_path_1,
            GetSharePathFromFileSystemId(file_system_id_1));
  EXPECT_EQ(expected_share_path_2,
            GetSharePathFromFileSystemId(file_system_id_2));
  EXPECT_EQ(expected_share_path_3,
            GetSharePathFromFileSystemId(file_system_id_3));
}

TEST_F(SmbFileSystemIdTest, IsKerberosChromadReturnsCorrectly) {
  const std::string kerberos_file_system_id =
      "13@@smb://192.168.0.1/test@@kerberos_chromad";
  const std::string non_kerberos_file_system_id = "12@@smb://192.168.0.0/test";

  EXPECT_TRUE(IsKerberosChromadFileSystemId(kerberos_file_system_id));
  EXPECT_FALSE(IsKerberosChromadFileSystemId(non_kerberos_file_system_id));
}

TEST_F(SmbFileSystemIdTest, GetUserFromFileSystemId) {
  const std::string file_system_id_1 = base::StrCat(
      {"EFAFF3864D0FE389@@smb://192.168.0.1/test@@user=", kTestUsername});
  const std::string user_workgroup =
      base::StrCat({kTestUsername, "@", kTestWorkgroup});
  const std::string file_system_id_2 = base::StrCat(
      {"EFAFF3864D0FE389@@smb://192.168.0.1/test@@user=", user_workgroup});

  base::Optional<std::string> actual_user =
      GetUserFromFileSystemId(file_system_id_1);
  ASSERT_TRUE(actual_user);
  EXPECT_EQ(kTestUsername, *actual_user);

  actual_user = GetUserFromFileSystemId(file_system_id_2);
  ASSERT_TRUE(actual_user);
  EXPECT_EQ(user_workgroup, *actual_user);
}

TEST_F(SmbFileSystemIdTest, GetUserFromFileSystemId_NoUser) {
  const std::string file_system_id_1 =
      "EFAFF3864D0FE389@@smb://192.168.0.1/test";
  const std::string file_system_id_2 =
      "EFAFF3864D0FE389@@smb://192.168.0.1/test@@kerberos_chromad";
  const std::string file_system_id_3 =
      "EFAFF3864D0FE389@@smb://192.168.0.1/test@@unrecognised_field";

  EXPECT_FALSE(GetUserFromFileSystemId(file_system_id_1));
  EXPECT_FALSE(GetUserFromFileSystemId(file_system_id_2));
  EXPECT_FALSE(GetUserFromFileSystemId(file_system_id_3));
}

}  // namespace smb_client
}  // namespace chromeos
