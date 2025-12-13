// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/unexportable_key_provider_config.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/common/chrome_version.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unexportable_keys {

namespace {

constexpr std::string_view kKeychainAccessGroup = MAC_TEAM_IDENTIFIER_STRING
    "." MAC_BUNDLE_IDENTIFIER_STRING ".unexportable-keys";

// Hex hash of "/user/data/dir"
constexpr std::string_view kUserDataDirHash = "af935f0dbf2111a4";

// Hex hash of u64{0} (Unix Epoch)
constexpr std::string_view kUnixEpochHash = "af5570f5a1810b7a";

// Hex hash of u64{1} (Unix Epoch + 1ms)
constexpr std::string_view kOneMillisecondAfterUnixEpochHash =
    "7c9fa136d4413fa6";

class UnexportableKeyProviderConfigTest : public testing::Test {
 public:
  UnexportableKeyProviderConfigTest() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UnexportableKeyProviderConfigTest, ForUserDataDir) {
  base::FilePath user_data_dir("/user/data/dir");
  const crypto::UnexportableKeyProvider::Config config =
      GetConfigForUserDataDir(user_data_dir);
  EXPECT_EQ(config.keychain_access_group, kKeychainAccessGroup);
  EXPECT_EQ(config.application_tag, base::JoinString(
                                        {
                                            kKeychainAccessGroup,
                                            kUserDataDirHash,
                                        },
                                        "."));
}

TEST_F(UnexportableKeyProviderConfigTest, ForProfilePath) {
  base::FilePath profile_path("/user/data/dir/test_profile");
  const crypto::UnexportableKeyProvider::Config config =
      GetConfigForProfilePath(profile_path);
  EXPECT_EQ(config.keychain_access_group, kKeychainAccessGroup);
  EXPECT_EQ(config.application_tag, base::JoinString(
                                        {
                                            kKeychainAccessGroup,
                                            kUserDataDirHash,
                                            "test_profile",
                                        },
                                        "."));
}

TEST_F(UnexportableKeyProviderConfigTest, ForOriginalProfile) {
  TestingProfile profile(base::FilePath("/user/data/dir/test_profile"));
  const crypto::UnexportableKeyProvider::Config config =
      GetConfigForProfile(profile);
  EXPECT_EQ(config.keychain_access_group, kKeychainAccessGroup);
  EXPECT_EQ(config.application_tag, base::JoinString(
                                        {
                                            kKeychainAccessGroup,
                                            kUserDataDirHash,
                                            "test_profile",
                                            kUnixEpochHash,
                                        },
                                        "."));
}

TEST_F(UnexportableKeyProviderConfigTest, ForOffTheRecordProfile) {
  TestingProfile profile(base::FilePath("/user/data/dir/test_profile"));
  Profile* otr_profile = profile.GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  otr_profile->SetCreationTimeForTesting(base::Time::UnixEpoch() +
                                         base::Milliseconds(1));

  const crypto::UnexportableKeyProvider::Config config =
      GetConfigForProfile(*otr_profile);
  EXPECT_EQ(config.keychain_access_group, kKeychainAccessGroup);
  EXPECT_EQ(config.application_tag, base::JoinString(
                                        {
                                            kKeychainAccessGroup,
                                            kUserDataDirHash,
                                            "test_profile",
                                            kOneMillisecondAfterUnixEpochHash,
                                        },
                                        "."));
}

TEST_F(UnexportableKeyProviderConfigTest, ForProfileAndPurpose) {
  TestingProfile profile(base::FilePath("/user/data/dir/test_profile"));

  const crypto::UnexportableKeyProvider::Config lst_config =
      GetConfigForProfileAndPurpose(profile, KeyPurpose::kRefreshTokenBinding);
  EXPECT_EQ(lst_config.keychain_access_group, kKeychainAccessGroup);
  EXPECT_EQ(lst_config.application_tag, base::JoinString(
                                            {
                                                kKeychainAccessGroup,
                                                kUserDataDirHash,
                                                "test_profile",
                                                kUnixEpochHash,
                                                "lst",
                                            },
                                            "."));

  const crypto::UnexportableKeyProvider::Config dbsc_config =
      GetConfigForProfileAndPurpose(profile,
                                    KeyPurpose::kDeviceBoundSessionCredentials);
  EXPECT_EQ(dbsc_config.keychain_access_group, kKeychainAccessGroup);
  EXPECT_EQ(dbsc_config.application_tag, base::JoinString(
                                             {
                                                 kKeychainAccessGroup,
                                                 kUserDataDirHash,
                                                 "test_profile",
                                                 kUnixEpochHash,
                                                 "dbsc",
                                             },
                                             "."));

  const crypto::UnexportableKeyProvider::Config dbsc_prototype_config =
      GetConfigForProfileAndPurpose(
          profile, KeyPurpose::kDeviceBoundSessionCredentialsPrototype);
  EXPECT_EQ(dbsc_prototype_config.keychain_access_group, kKeychainAccessGroup);
  EXPECT_EQ(dbsc_prototype_config.application_tag, base::JoinString(
                                                       {
                                                           kKeychainAccessGroup,
                                                           kUserDataDirHash,
                                                           "test_profile",
                                                           kUnixEpochHash,
                                                           "dbsc-prototype",
                                                       },
                                                       "."));
}

}  // namespace

}  // namespace unexportable_keys
