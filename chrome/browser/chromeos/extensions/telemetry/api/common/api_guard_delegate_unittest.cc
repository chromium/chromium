// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"  // nogncheck
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "chromeos/ash/components/mojo_service_manager/fake_mojo_service_manager.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "components/account_id/account_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_urls.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

struct ExtensionInfoTestParams {
  ExtensionInfoTestParams(const std::string& extension_id,
                          const std::string& matches_origin,
                          const std::string& manufacturer)
      : extension_id(extension_id),
        matches_origin(matches_origin),
        manufacturer(manufacturer) {}
  ExtensionInfoTestParams(const ExtensionInfoTestParams& other) = default;
  ~ExtensionInfoTestParams() = default;

  const std::string extension_id;
  const std::string matches_origin;
  const std::string manufacturer;
};

constexpr char kGoogleExtensionId[] = "gogonhoemckpdpadfnjnpgbjpbjnodgc";
constexpr char kGoogleAllowedUrlPattern[] =
    "*://googlechromelabs.github.io/cros-sample-telemetry-extension/test-page/"
    "*";
constexpr char kUserEmail[] = "user@example.com";
constexpr char kSecondUserEmail[] = "second@example.com";
constexpr GaiaId::Literal kSecondUserGaiaId("fakegaia2");

const std::vector<ExtensionInfoTestParams>& GetAllExtensionInfoTestParams() {
  static const base::NoDestructor<std::vector<ExtensionInfoTestParams>> val({
      // Make sure the Google extension is allowed for every OEM.
      ExtensionInfoTestParams(
          /*extension_id=*/kGoogleExtensionId,
          /*matches_origin=*/kGoogleAllowedUrlPattern,
          /*manufacturer=*/"HP"),
      ExtensionInfoTestParams(
          /*extension_id=*/kGoogleExtensionId,
          /*matches_origin=*/kGoogleAllowedUrlPattern,
          /*manufacturer=*/"ASUS"),
      ExtensionInfoTestParams(
          /*extension_id=*/kGoogleExtensionId,
          /*matches_origin=*/kGoogleAllowedUrlPattern,
          /*manufacturer=*/"Acer"),
      ExtensionInfoTestParams(
          /*extension_id=*/kGoogleExtensionId,
          /*matches_origin=*/kGoogleAllowedUrlPattern,
          /*manufacturer=*/"Lenovo"),
      // Make sure the extensions of each OEM are allowed on their device.
      ExtensionInfoTestParams(
          /*extension_id=*/"alnedpmllcfpgldkagbfbjkloonjlfjb",
          /*matches_origin=*/"https://hpcs-appschr.hpcloud.hp.com/*",
          /*manufacturer=*/"HP"),
      ExtensionInfoTestParams(
          /*extension_id=*/"hdnhcpcfohaeangjpkcjkgmgmjanbmeo",
          /*matches_origin=*/"https://dlcdnccls.asus.com/*",
          /*manufacturer=*/"ASUS"),
      ExtensionInfoTestParams(
          /*extension_id=*/"aoefhlbfcighemjpchndkhonjfjoehnm",
          /*matches_origin=*/"https://acerpartners.com/*",
          /*manufacturer=*/"Acer"),
      ExtensionInfoTestParams(
          /*extension_id=*/"mconamggkmbalafmibfjlcmimnlbgmlb",
          /*matches_origin=*/"https://chromebookdiags.lenovo.com/*",
          /*manufacturer=*/"Lenovo"),
      ExtensionInfoTestParams(
          /*extension_id=*/"hoalheabnfilagemmocodoambpgngdcd",
          /*matches_origin=*/"https://cscpwa.asus.com/*",
          /*manufacturer=*/"ASUS"),
  });
  return *val;
}

// Tests that Chrome OS System Extensions must fulfill the requirements to
// access Telemetry Extension APIs. All tests are parameterized with the
// following parameters:
// * |extension_id| - id of the extension under test.
// * |matches_origin| - externally_connectable's matches entry of the
//                      extension's manifest.json.
// * |manufacturer| - manufacturer of the device.
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
    ash::cros_healthd::FakeCrosHealthd::Initialize();
    BrowserWithTestWindowTest::SetUp();

    CreateExtension();

    // Make sure device manufacturer is allowlisted.
    SetDeviceManufacturer(manufacturer());
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    ash::cros_healthd::FakeCrosHealthd::Shutdown();
  }

  std::optional<std::string> GetDefaultProfileName() override {
    return kUserEmail;
  }

 protected:
  virtual TestingProfile* target_profile() { return profile(); }

  extensions::ExtensionId extension_id() const {
    return GetParam().extension_id;
  }

  std::string matches_origin() const { return GetParam().matches_origin; }

  std::string manufacturer() const { return GetParam().manufacturer; }

  const extensions::Extension* target_extension() {
    return target_extension_.get();
  }

  std::unique_ptr<ApiGuardDelegate> CreateApiGuardDelegate() {
    return ApiGuardDelegate::Factory::CreateForTesting(base::BindRepeating(
        &ApiGuardDelegateTest::IsAppUiOpenAndSecure, base::Unretained(this)));
  }

  void SetAppUiOpenAndSecure(bool is_open_and_secure) {
    is_app_ui_open_and_secure_ = is_open_and_secure;
  }

  void SetUserAsOwner() {
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail(kUserEmail);
    user_manager::UserManager::Get()->SetOwnerId(account_id);
  }

  void SetDeviceManufacturer(const std::string& manufacturer) {
    HardwareInfoDelegate::Get().ClearCacheForTesting();
    auto os_info = ash::cros_healthd::mojom::OsInfo::New();
    os_info->os_version = ash::cros_healthd::mojom::OsVersion::New();
    os_info->oem_name = manufacturer;

    auto system_info = ash::cros_healthd::mojom::SystemInfo::New();
    system_info->os_info = std::move(os_info);

    auto telemetry_info = ash::cros_healthd::mojom::TelemetryInfo::New();
    telemetry_info->system_result =
        ash::cros_healthd::mojom::SystemResult::NewSystemInfo(
            std::move(system_info));

    auto* fake_cros_healthd = ash::cros_healthd::FakeCrosHealthd::Get();
    fake_cros_healthd->SetProbeTelemetryInfoResponseForTesting(
        std::move(telemetry_info));
  }

 private:
  bool IsAppUiOpenAndSecure(content::BrowserContext* context,
                            const extensions::Extension* extension) {
    EXPECT_EQ(context, target_profile());
    EXPECT_EQ(extension, target_extension());
    return is_app_ui_open_and_secure_;
  }

  void CreateExtension() {
    target_extension_ =
        extensions::ExtensionBuilder("Test ChromeOS System Extension")
            .SetManifestKey("chromeos_system_extension", base::DictValue())
            .SetManifestKey(
                "externally_connectable",
                base::DictValue().Set(
                    "matches", base::ListValue().Append(matches_origin())))
            .SetID(extension_id())
            .SetLocation(extensions::mojom::ManifestLocation::kInternal)
            .Build();
  }

  bool is_app_ui_open_and_secure_ = false;
  ash::mojo_service_manager::FakeMojoServiceManager fake_service_manager_;
  scoped_refptr<const extensions::Extension> target_extension_;
};

TEST_P(ApiGuardDelegateTest, CurrentUserNotOwner) {
  // Make sure the current user is not the device owner.
  const AccountId regular_user = AccountId::FromUserEmail("regular@gmail.com");
  user_manager::UserManager::Get()->SetOwnerId(regular_user);

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not run by the device owner", error.value());
}

TEST_P(ApiGuardDelegateTest, OwnershipDelayed) {
  SetAppUiOpenAndSecure(true);
  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;

  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  // Trigger async ownership retrieval.
  SetUserAsOwner();

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

TEST_P(ApiGuardDelegateTest, AppNotOpenOrNotSecure) {
  SetUserAsOwner();
  SetAppUiOpenAndSecure(false);
  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateTest, ManufacturerNotAllowed) {
  SetUserAsOwner();
  SetAppUiOpenAndSecure(true);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
}

TEST_P(ApiGuardDelegateTest, SkipManufacturerCheck) {
  SetUserAsOwner();
  SetAppUiOpenAndSecure(true);
  // Append the switch to skip the manufacturer check.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kTelemetryExtensionSkipManufacturerCheckForTesting);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

TEST_P(ApiGuardDelegateTest, NoError) {
  SetUserAsOwner();
  SetAppUiOpenAndSecure(true);

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateTest,
                         testing::ValuesIn(GetAllExtensionInfoTestParams()));

class ApiGuardDelegateAffiliatedUserTest : public ApiGuardDelegateTest {
 public:
  ApiGuardDelegateAffiliatedUserTest() = default;
  ~ApiGuardDelegateAffiliatedUserTest() override = default;

 protected:
  void LogIn(std::string_view email, const GaiaId& gaia_id) override {
    BrowserWithTestWindowTest::LogIn(email, gaia_id);
    user_manager::UserManager::Get()->SetUserPolicyStatus(
        AccountId::FromUserEmailGaiaId(email, gaia_id),
        /*is_managed=*/true,
        /*is_affiliated=*/true);
  }
};

TEST_P(ApiGuardDelegateAffiliatedUserTest, ExtensionNotForceInstalled) {
  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not installed by the admin", error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, AppNotOpenOrNotSecure) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(target_profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  SetAppUiOpenAndSecure(false);

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, ManufacturerNotAllowed) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(target_profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  SetAppUiOpenAndSecure(true);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, NoError) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(target_profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  SetAppUiOpenAndSecure(true);

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());
  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateAffiliatedUserTest,
                         testing::ValuesIn(GetAllExtensionInfoTestParams()));

class ApiGuardDelegateShimlessRMAAppTest : public ApiGuardDelegateTest {
 public:
  ApiGuardDelegateShimlessRMAAppTest() = default;
  ~ApiGuardDelegateShimlessRMAAppTest() override = default;

  void SetUp() override {
    chromeos_system_extension_info_ =
        ScopedChromeOSSystemExtensionInfo::CreateForTesting();
    // TODO(b/293560424): Remove this override after we add some valid IWA id to
    // the allowlist.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        chromeos::switches::kTelemetryExtensionIwaIdOverrideForTesting,
        "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic");
    chromeos_system_extension_info_->ApplyCommandLineSwitchesForTesting();

    // Above overrides need to be done before creating extensions.
    ApiGuardDelegateTest::SetUp();

    shimless_profile_ =
        CreateProfile(ash::kShimlessRmaAppBrowserContextBaseName);
  }

  void TearDown() override {
    shimless_profile_ = nullptr;
    ApiGuardDelegateTest::TearDown();
  }

 protected:
  TestingProfile* target_profile() override { return shimless_profile_; }

  // BrowserWithTestWindowTest overrides.
  std::optional<std::string> GetDefaultProfileName() override {
    // Shimless RMA runs without a user session or a Browser window. Returning
    // std::nullopt skips the default user login, profile creation, and Browser
    // instantiation in BrowserWithTestWindowTest::SetUp().
    return std::nullopt;
  }

 private:
  raw_ptr<TestingProfile> shimless_profile_ = nullptr;
  std::unique_ptr<ScopedChromeOSSystemExtensionInfo>
      chromeos_system_extension_info_;
};

TEST_P(ApiGuardDelegateShimlessRMAAppTest, IwaNotOpen) {
  SetAppUiOpenAndSecure(false);
  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion app UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateShimlessRMAAppTest, ManufacturerNotAllowed) {
  SetAppUiOpenAndSecure(true);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
}

TEST_P(ApiGuardDelegateShimlessRMAAppTest, NoError) {
  SetAppUiOpenAndSecure(true);

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());
  ASSERT_TRUE(future.Wait());
  std::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

TEST_P(ApiGuardDelegateTest, OwnerCheckUsesCallingProfile) {
  SetAppUiOpenAndSecure(true);

  // Log in a second user, mark them as the device owner and make them the
  // active user. The calling profile still belongs to the first user.
  const AccountId second_user =
      AccountId::FromUserEmailGaiaId(kSecondUserEmail, kSecondUserGaiaId);
  LogIn(kSecondUserEmail, kSecondUserGaiaId);
  user_manager::UserManager::Get()->SetOwnerId(second_user);
  user_manager::UserManager::Get()->SwitchActiveUser(second_user);
  ASSERT_TRUE(user_manager::UserManager::Get()->IsCurrentUserOwner());

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not run by the device owner", error.value());
}

TEST_P(ApiGuardDelegateTest, AffiliationCheckUsesCallingProfile) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(target_profile()->GetTestingPrefService());
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }
  SetAppUiOpenAndSecure(true);

  // Log in a second user, mark them as the device owner and affiliated, and
  // make them the active user. The calling profile still belongs to the first
  // (unaffiliated, non-owner) user.
  const AccountId second_user =
      AccountId::FromUserEmailGaiaId(kSecondUserEmail, kSecondUserGaiaId);
  LogIn(kSecondUserEmail, kSecondUserGaiaId);
  user_manager::UserManager::Get()->SetOwnerId(second_user);
  user_manager::UserManager::Get()->SetUserPolicyStatus(second_user,
                                                        /*is_managed=*/true,
                                                        /*is_affiliated=*/true);
  user_manager::UserManager::Get()->SwitchActiveUser(second_user);

  auto api_guard_delegate = CreateApiGuardDelegate();
  base::test::TestFuture<std::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(target_profile(), target_extension(),
                                   future.GetCallback());

  std::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not run by the device owner", error.value());
}

INSTANTIATE_TEST_SUITE_P(
    IWA,
    ApiGuardDelegateShimlessRMAAppTest,
    testing::Values(ExtensionInfoTestParams(
        /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
        /*matches_origin=*/
        "isolated-app://"
        "pt2jysa7yu326m2cbu5mce4rrajvguagronrsqwn5dhbaris6eaaaaic/*",
        /*manufacturer=*/"HP")));

}  // namespace chromeos
