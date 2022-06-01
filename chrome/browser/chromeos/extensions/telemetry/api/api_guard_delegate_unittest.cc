// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/fake_hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

struct ExtensionInfoTestParams {
  ExtensionInfoTestParams(const std::string& extension_id,
                          const std::string& pwa_page_url,
                          const std::string& matches_origin)
      : extension_id(extension_id),
        pwa_page_url(pwa_page_url),
        matches_origin(matches_origin) {}
  ExtensionInfoTestParams(const ExtensionInfoTestParams& other) = default;
  ~ExtensionInfoTestParams() = default;

  const std::string extension_id;
  const std::string pwa_page_url;
  const std::string matches_origin;
};

const std::vector<ExtensionInfoTestParams> kAllExtensionInfoTestParams{
    ExtensionInfoTestParams(
        /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
        /*pwa_page_url=*/"http://www.google.com",
        /*matches_origin=*/"*://www.google.com/*"),
    ExtensionInfoTestParams(
        /*extension_id=*/"alnedpmllcfpgldkagbfbjkloonjlfjb",
        /*pwa_page_url=*/"https://hpcs-appschr.hpcloud.hp.com",
        /*matches_origin=*/"https://hpcs-appschr.hpcloud.hp.com/*")};

// Tests that Chrome OS System Extensions must fulfill the requirements to
// access Telemetry Extension APIs. All tests are parameterized with the
// following parameters:
// * |extension_id| - id of the extension under test.
// * |pwa_page_url| - page URL of the PWA associated with the extension's id.
// * |matches_origin| - externally_connectable's matches entry of the
//                      extension's manifest.json.
// Note: All tests must be defined using the TEST_P macro and must use the
// INSTANTIATE_TEST_SUITE_P macro to instantiate the test suite.
class ApiGuardDelegateTest
    : public BrowserWithTestWindowTest,
      public testing::WithParamInterface<ExtensionInfoTestParams> {
 public:
  ApiGuardDelegateTest() = default;
  ~ApiGuardDelegateTest() override = default;

  // BrowserWithTestWindowTest:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    CreateExtension();
    // Make sure device manufacturer is allowlisted.
    SetDeviceManufacturer("HP");
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    AddUserAndLogIn();
  }

  void TearDown() override {
    // Explicitly removing the user is required; otherwise ProfileHelper keeps a
    // dangling pointer to the User.
    // TODO(b/208629291): Consider removing all users from ProfileHelper in the
    // destructor of ash::FakeChromeUserManager.
    GetFakeUserManager()->RemoveUserFromList(
        GetFakeUserManager()->GetActiveUser()->GetAccountId());
    scoped_user_manager_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  std::string extension_id() const { return GetParam().extension_id; }

  std::string pwa_page_url() const { return GetParam().pwa_page_url; }

  std::string matches_origin() const { return GetParam().matches_origin; }

  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  const extensions::Extension* extension() { return extension_.get(); }

  virtual void AddUserAndLogIn() {
    auto* const user_manager = GetFakeUserManager();
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail("user@example.com");
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);
    user_manager->SwitchActiveUser(account_id);
    user_manager->SetOwnerId(account_id);
  }

  void SetDeviceManufacturer(std::string manufacturer) {
    hardware_info_delegate_factory_ =
        std::make_unique<FakeHardwareInfoDelegate::Factory>(manufacturer);
    HardwareInfoDelegate::Factory::SetForTesting(
        hardware_info_delegate_factory_.get());
  }

 private:
  void CreateExtension() {
    extension_ =
        extensions::ExtensionBuilder("Test ChromeOS System Extension")
            .SetManifestVersion(3)
            .SetManifestKey("chromeos_system_extension",
                            extensions::DictionaryBuilder().Build())
            .SetManifestKey("externally_connectable",
                            extensions::DictionaryBuilder()
                                .Set("matches", extensions::ListBuilder()
                                                    .Append(matches_origin())
                                                    .Build())
                                .Build())
            .SetID(extension_id())
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
  }

  scoped_refptr<const extensions::Extension> extension_;
  std::unique_ptr<HardwareInfoDelegate::Factory>
      hardware_info_delegate_factory_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_P(ApiGuardDelegateTest, CurrentUserNotOwner) {
  auto* const user_manager = GetFakeUserManager();
  // Make sure the current user is not the device owner.
  const AccountId regular_user = AccountId::FromUserEmail("regular@gmail.com");
  user_manager->SetOwnerId(regular_user);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("This extension is not run by the device owner", future.Get());
}

TEST_P(ApiGuardDelegateTest, PwaNotOpen) {
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("Companion PWA UI is not open", future.Get());
}

TEST_P(ApiGuardDelegateTest, ManufacturerNotAllowed) {
  // Open the PWA page url to bypass IsPwaUiOpen() check.
  AddTab(browser(), GURL(pwa_page_url()));

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("GOOGLE");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            future.Get());
}

TEST_P(ApiGuardDelegateTest, NoError) {
  // Open the PWA page url to bypass IsPwaUiOpen() check.
  AddTab(browser(), GURL(pwa_page_url()));

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty()) << future.Get();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateTest,
                         testing::ValuesIn(kAllExtensionInfoTestParams));

class ApiGuardDelegateAffiliatedUserTest : public ApiGuardDelegateTest {
 public:
  ApiGuardDelegateAffiliatedUserTest() = default;
  ~ApiGuardDelegateAffiliatedUserTest() override = default;

 protected:
  void AddUserAndLogIn() override {
    auto* const user_manager = GetFakeUserManager();
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail("user@example.com");
    user_manager->AddUserWithAffiliation(account_id, /*is_affiliated=*/true);
    user_manager->LoginUser(account_id);
    user_manager->SwitchActiveUser(account_id);
  }
};

TEST_P(ApiGuardDelegateAffiliatedUserTest, ExtensionNotForceInstalled) {
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("This extension is not installed by the admin", future.Get());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, PwaNotOpen) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("Companion PWA UI is not open", future.Get());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, ManufacturerNotAllowed) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  // Open the PWA page url to bypass IsPwaUiOpen() check.
  AddTab(browser(), GURL(pwa_page_url()));

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("GOOGLE");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            future.Get());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, NoError) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  // Open the PWA page url to bypass IsPwaUiOpen() check.
  AddTab(browser(), GURL(pwa_page_url()));

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<std::string> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_TRUE(future.Get().empty()) << future.Get();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateAffiliatedUserTest,
                         testing::ValuesIn(kAllExtensionInfoTestParams));

}  // namespace chromeos
