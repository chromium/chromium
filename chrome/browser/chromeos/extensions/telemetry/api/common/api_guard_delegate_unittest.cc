// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/api_guard_delegate.h"
#include "chrome/browser/chromeos/extensions/telemetry/api/common/fake_hardware_info_delegate.h"
#include "chrome/browser/extensions/extension_management_test_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_urls.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace chromeos {

struct ExtensionInfoTestParams {
  ExtensionInfoTestParams(const std::string& extension_id,
                          const std::string& pwa_page_url,
                          const std::string& matches_origin,
                          const std::string& manufacturer)
      : extension_id(extension_id),
        pwa_page_url(pwa_page_url),
        matches_origin(matches_origin),
        manufacturer(manufacturer) {}
  ExtensionInfoTestParams(const ExtensionInfoTestParams& other) = default;
  ~ExtensionInfoTestParams() = default;

  const std::string extension_id;
  const std::string pwa_page_url;
  const std::string matches_origin;
  const std::string manufacturer;
};

const std::vector<ExtensionInfoTestParams> kAllExtensionInfoTestParams{
    // Make sure the Google extension is allowed for every OEM.
    ExtensionInfoTestParams(
        /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
        /*pwa_page_url=*/"https://googlechromelabs.github.io/",
        /*matches_origin=*/"*://googlechromelabs.github.io/*",
        /*manufacturer=*/"HP"),
    ExtensionInfoTestParams(
        /*extension_id=*/"gogonhoemckpdpadfnjnpgbjpbjnodgc",
        /*pwa_page_url=*/"https://googlechromelabs.github.io/",
        /*matches_origin=*/"*://googlechromelabs.github.io/*",
        /*manufacturer=*/"ASUS"),
    // Make sure the extensions of each OEM are allowed on their device.
    ExtensionInfoTestParams(
        /*extension_id=*/"alnedpmllcfpgldkagbfbjkloonjlfjb",
        /*pwa_page_url=*/"https://hpcs-appschr.hpcloud.hp.com",
        /*matches_origin=*/"https://hpcs-appschr.hpcloud.hp.com/*",
        /*manufacturer=*/"HP"),
    ExtensionInfoTestParams(
        /*extension_id=*/"hdnhcpcfohaeangjpkcjkgmgmjanbmeo",
        /*pwa_page_url=*/
        "https://dlcdnccls.asus.com/app/myasus_for_chromebook/ ",
        /*matches_origin=*/"https://dlcdnccls.asus.com/*",
        /*manufacturer=*/"ASUS"),
};

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
    SetDeviceManufacturer(manufacturer());

#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    AddUserAndLogIn();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto params = crosapi::mojom::BrowserInitParams::New();
    params->is_current_user_device_owner = true;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));

    profile()->SetIsMainProfile(true);
    ASSERT_TRUE(profile()->IsMainProfile());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Explicitly removing the user is required; otherwise ProfileHelper keeps a
    // dangling pointer to the User.
    // TODO(b/208629291): Consider removing all users from ProfileHelper in the
    // destructor of ash::FakeChromeUserManager.
    GetFakeUserManager()->RemoveUserFromList(
        GetFakeUserManager()->GetActiveUser()->GetAccountId());
    scoped_user_manager_.reset();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  std::string extension_id() const { return GetParam().extension_id; }

  std::string pwa_page_url() const { return GetParam().pwa_page_url; }

  std::string matches_origin() const { return GetParam().matches_origin; }

  std::string manufacturer() const { return GetParam().manufacturer; }

  const extensions::Extension* extension() { return extension_.get(); }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  virtual void AddUserAndLogIn() {
    auto* const user_manager = GetFakeUserManager();
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail("user@example.com");
    user_manager->AddUser(account_id);
    user_manager->LoginUser(account_id);
    user_manager->SwitchActiveUser(account_id);
    user_manager->SetOwnerId(account_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void SetDeviceManufacturer(std::string manufacturer) {
    hardware_info_delegate_factory_ =
        std::make_unique<FakeHardwareInfoDelegate::Factory>(manufacturer);
    HardwareInfoDelegate::Factory::SetForTesting(
        hardware_info_delegate_factory_.get());
  }

  void OpenPwaUrlAndSetCertificateWithStatus(net::CertStatus cert_status) {
    const base::FilePath certs_dir = net::GetTestCertsDirectory();
    scoped_refptr<net::X509Certificate> test_cert(
        net::ImportCertFromFile(certs_dir, "ok_cert.pem"));
    ASSERT_TRUE(test_cert);

    // Open the PWA page url and set valid certificate to bypass the
    // IsPwaUiOpenAndSecure() check.
    AddTab(browser(), GURL(pwa_page_url()));

    // AddTab() adds a new tab at index 0.
    auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
    auto* entry = web_contents->GetController().GetVisibleEntry();
    content::SSLStatus& ssl = entry->GetSSL();
    ssl.certificate = test_cert;
    ssl.cert_status = cert_status;
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_P(ApiGuardDelegateTest, CurrentUserNotOwner) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* const user_manager = GetFakeUserManager();
  // Make sure the current user is not the device owner.
  const AccountId regular_user = AccountId::FromUserEmail("regular@gmail.com");
  user_manager->SetOwnerId(regular_user);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto params = crosapi::mojom::BrowserInitParams::New();
  params->is_current_user_device_owner = false;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not run by the device owner", error.value());
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_P(ApiGuardDelegateTest, CurrentUserOwnerButNotMainLacrosProfile) {
  // Don't set the current profile as the main profile.
  profile()->SetIsMainProfile(false);
  ASSERT_FALSE(profile()->IsMainProfile());

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not run by the device owner", error.value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

TEST_P(ApiGuardDelegateTest, PwaNotOpen) {
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion PWA UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateTest, PwaIsOpenButNotSecure) {
  OpenPwaUrlAndSetCertificateWithStatus(
      /*cert_status=*/net::CERT_STATUS_INVALID);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion PWA UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateTest, ManufacturerNotAllowed) {
  OpenPwaUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
}

TEST_P(ApiGuardDelegateTest, SkipManufacturerCheck) {
  OpenPwaUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);
  // Append the switch to skip the manufacturer check.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kTelemetryExtensionSkipManufacturerCheckForTesting);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

TEST_P(ApiGuardDelegateTest, NoError) {
  OpenPwaUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateTest,
                         testing::ValuesIn(kAllExtensionInfoTestParams));

class ApiGuardDelegateAffiliatedUserTest : public ApiGuardDelegateTest {
 public:
  ApiGuardDelegateAffiliatedUserTest() = default;
  ~ApiGuardDelegateAffiliatedUserTest() override = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetUp() override {
    ApiGuardDelegateTest::SetUp();

    // Make sure the main user is affiliated.
    auto init_params = crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
    chromeos::BrowserInitParams::SetInitParamsForTests(std::move(init_params));
    ASSERT_TRUE(policy::PolicyLoaderLacros::IsMainUserAffiliated());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void AddUserAndLogIn() override {
    auto* const user_manager = GetFakeUserManager();
    // Make sure the current user is affiliated.
    const AccountId account_id = AccountId::FromUserEmail("user@example.com");
    user_manager->AddUserWithAffiliation(account_id, /*is_affiliated=*/true);
    user_manager->LoginUser(account_id);
    user_manager->SwitchActiveUser(account_id);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_P(ApiGuardDelegateAffiliatedUserTest, ExtensionNotForceInstalled) {
  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not installed by the admin", error.value());
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
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion PWA UI is not open or not secure", error.value());
}

TEST_P(ApiGuardDelegateAffiliatedUserTest, PwaIsOpenButNotSecure) {
  {
    extensions::ExtensionManagementPrefUpdater<
        sync_preferences::TestingPrefServiceSyncable>
        updater(profile()->GetTestingPrefService());
    // Make sure the extension is marked as force-installed.
    updater.SetIndividualExtensionAutoInstalled(
        extension_id(), extension_urls::kChromeWebstoreUpdateURL,
        /*forced=*/true);
  }

  OpenPwaUrlAndSetCertificateWithStatus(
      /*cert_status=*/net::CERT_STATUS_INVALID);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("Companion PWA UI is not open or not secure", error.value());
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

  OpenPwaUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  // Make sure device manufacturer is not allowed.
  SetDeviceManufacturer("NOT_ALLOWED");

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());

  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  ASSERT_TRUE(error.has_value());
  EXPECT_EQ("This extension is not allowed to access the API on this device",
            error.value());
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

  OpenPwaUrlAndSetCertificateWithStatus(/*cert_status=*/net::OK);

  auto api_guard_delegate = ApiGuardDelegate::Factory::Create();
  base::test::TestFuture<absl::optional<std::string>> future;
  api_guard_delegate->CanAccessApi(profile(), extension(),
                                   future.GetCallback());
  ASSERT_TRUE(future.Wait());
  absl::optional<std::string> error = future.Get();
  EXPECT_FALSE(error.has_value()) << error.value();
}

INSTANTIATE_TEST_SUITE_P(All,
                         ApiGuardDelegateAffiliatedUserTest,
                         testing::ValuesIn(kAllExtensionInfoTestParams));

}  // namespace chromeos
