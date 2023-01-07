// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"

#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/box_api_call_test_helper.h"
#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"
#include "chrome/browser/enterprise/connectors/file_system/test_helper.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using download::DownloadInterruptReason;
using testing::_;
using testing::Invoke;
using testing::Return;

namespace enterprise_connectors {

constexpr char kBox[] = "box";
constexpr char kAccountNameKey[] = "name";
constexpr char kAccountLoginKey[] = "login";
constexpr char kAccountName[] = "Jane Doe";
constexpr char kAccountLogin[] = "janedoe@example.com";
constexpr char kDefaultFolderName[] = "ChromeDownloads";
constexpr char kDefaultFolderId[] = "13579";
constexpr char kDefaultFolderLink[] = "https://app.box.com/folder/13579";

base::DictionaryValue MakeAccountInfoDict(std::string name, std::string login) {
  base::DictionaryValue dict;
  dict.SetStringKey(kAccountNameKey, name);
  dict.SetStringKey(kAccountLoginKey, login);
  return dict;
}

class SigninExperienceForSettingsPageTest : public testing::Test {
 public:
  SigninExperienceForSettingsPageTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
  }

  void SetUp() override {
    // Need this for OAuth tokens since they are stored with encryption.
    OSCryptMocker::SetUp();

    feature_list_.InitWithFeatures({kFileSystemConnectorEnabled}, {});

    // Make sure that from the connectors manager point of view the file system
    // connector should be enabled.  So that the only thing that controls
    // whether the rename handler is used or not is the feature flag.
    profile_->GetPrefs()->Set(
        ConnectorPref(FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD),
        *base::JSONReader::Read(kWildcardSendDownloadToCloudPref));

    // Make sure we start each test case from a clean slate.
    ASSERT_TRUE(UnlinkAccount());
    ASSERT_TRUE(VerifyNoLinkedAccount());
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

 protected:
  bool LinkAccount(std::string service_provider,
                   std::string account_name,
                   std::string account_login,
                   std::string folder_id,
                   std::string folder_name) {
    std::string test_case_name =
        ::testing::UnitTest::GetInstance()->current_test_info()->name();
    SetFileSystemAccountInfo(prefs(), service_provider,
                             MakeAccountInfoDict(account_name, account_login));
    SetDefaultFolder(prefs(), service_provider, folder_id, folder_name);
    return SetFileSystemOAuth2Tokens(prefs(), service_provider,
                                     test_case_name + "_AToken",
                                     test_case_name + "_RToken");
  }

  bool VerifyLinkedAccount(std::string ref_account_name,
                           std::string ref_account_login,
                           std::string ref_folder_link,
                           std::string ref_folder_name) {
    auto settings = GetFileSystemSettings(profile());
    if (!settings.has_value()) {
      ADD_FAILURE() << "GetFileSystemSettings() failed";
      return false;
    }
    auto info =
        GetFileSystemConnectorLinkedAccountInfo(settings.value(), prefs());
    if (!info.has_value()) {
      ADD_FAILURE() << "Get linked account info failed";
      return false;
    }
    EXPECT_EQ(info->account_name, ref_account_name);
    EXPECT_EQ(info->account_login, ref_account_login);
    EXPECT_EQ(info->folder_name, ref_folder_name);
    EXPECT_EQ(info->folder_link, ref_folder_link);
    return true;
  }

  bool UnlinkAccount() {
    bool set_success = false;
    SetFileSystemConnectorAccountLinkForSettingsPage(
        /* enable_link = */ false, profile(),
        base::BindLambdaForTesting(
            [&set_success](bool success) { set_success = success; }));
    base::RunLoop().RunUntilIdle();

    return set_success;
  }

  bool VerifyNoLinkedAccount() {
    auto settings = GetFileSystemSettings(profile());
    if (!settings.has_value()) {
      ADD_FAILURE() << "GetFileSystemSettings() failed";
      return false;
    }
    if (!VerifyBothTokensCleared()) {
      ADD_FAILURE() << "VerifyBothTokensCleared() failed";
      return false;
    }
    auto info =
        GetFileSystemConnectorLinkedAccountInfo(settings.value(), prefs());
    if (info.has_value()) {
      ADD_FAILURE() << "Still has linked account";
      return false;
    }
    return true;
  }

  bool VerifyBothTokensCleared() {
    std::string atoken, rtoken;
    if (GetFileSystemOAuth2Tokens(prefs(), kBox, &atoken, &rtoken) &&
        (!atoken.empty() || !rtoken.empty())) {
      ADD_FAILURE() << "Access Token: " << atoken
                    << "\nRefresh Token: " << rtoken;
      return false;
    }
    bool both_cleared = atoken.empty() && rtoken.empty();
    EXPECT_TRUE(both_cleared)
        << "Access Token: " << atoken << "\nRefresh Token: " << rtoken;
    return both_cleared;
  }

  Profile* profile() { return profile_; }
  PrefService* prefs() { return profile_->GetPrefs(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SigninExperienceForSettingsPageTest, FromLinked_ToUnlinked) {
  // Setup info for a linked account and verify.
  ASSERT_TRUE(LinkAccount(kBox, kAccountName, kAccountLogin, kDefaultFolderId,
                          kDefaultFolderName));
  ASSERT_TRUE(VerifyLinkedAccount(kAccountName, kAccountLogin,
                                  kDefaultFolderLink, kDefaultFolderName));
  // Unlink account and verify.
  ASSERT_TRUE(UnlinkAccount());
  ASSERT_TRUE(VerifyNoLinkedAccount());
}

TEST_F(SigninExperienceForSettingsPageTest, FromUnlinked_Unchanged) {
  // Already initialized to unlinked and verified in SetUp().
  ASSERT_TRUE(UnlinkAccount());
  ASSERT_TRUE(VerifyNoLinkedAccount());
}

}  // namespace enterprise_connectors
