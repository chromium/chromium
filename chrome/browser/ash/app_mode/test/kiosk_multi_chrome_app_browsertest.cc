// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <vector>

#include "base/check_deref.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test_kiosk_extension_builder.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/test_app_data_load_waiter.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CurrentProfile;
using kiosk::test::InstalledChromeAppVersion;
using kiosk::test::IsChromeAppInstalled;

namespace {

// Testing apps for testing kiosk multi-app feature. All the crx files are in
//    chrome/test/data/chromeos/app_mode/webstore/downloads.

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/primary_app
constexpr char kPrimaryAppId[] = "fclmjfpgiaifbnbnlpmdjhicolkapihc";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app_1
constexpr char kSecondaryApp1Id[] = "elbhpkeieolijdlflcplbbabceggjknh";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app_2
constexpr char kSecondaryApp2Id[] = "coamgmmgmjeeaodkbpdajekljacgfhkc";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app_3
constexpr char kSecondaryApp3Id[] = "miccbahcahimnejpdoaafjeolookhoem";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/
//         secondary_extensions_1
constexpr char kSecondaryExtensionId[] = "pegeblegnlhnpgghhjblhchdllfijodp";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/
//         shared_module_primary_app
constexpr char kSharedModulePrimaryAppId[] = "kidkeddeanfhailinhfokehpolmfdppa";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/secondary_app
constexpr char kSecondaryAppId[] = "ffceghmcpipkneddgikbgoagnheejdbf";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/shared_module
constexpr char kSharedModuleId[] = "hpanhkopkhnkpcmnedlnjmkfafmlamak";

// Source files are in
//     chrome/test/data/chromeos/app_mode/multi_app_kiosk/src/
//         secondary_extension
constexpr char kSecondaryExtId[] = "meaknlbicgahoejcchpnkenkmbekcddf";

extensions::Manifest::Type ManifestType(const extensions::ExtensionId id) {
  const auto& app_or_extension =
      CHECK_DEREF(extensions::ExtensionRegistry::Get(&CurrentProfile())
                      ->GetInstalledExtension(id));
  return app_or_extension.GetType();
}

struct TestAppInfo {
  TestAppInfo(std::string_view id,
              std::string_view version,
              extensions::Manifest::Type type)
      : id(std::string(id)),
        version(std::string(version)),
        crx_filename(base::StrCat({id, "-", version, ".crx"})),
        type(type) {}
  TestAppInfo(const TestAppInfo&) = default;
  TestAppInfo& operator=(const TestAppInfo&) = default;
  TestAppInfo(TestAppInfo&&) = default;
  TestAppInfo& operator=(TestAppInfo&&) = default;
  ~TestAppInfo() = default;

  std::string id;
  std::string version;
  std::string crx_filename;
  extensions::Manifest::Type type;
};

void SetupAppDetailInFakeCws(FakeCWS& fake_cws, const TestAppInfo& app) {
  scoped_refptr<const extensions::Extension> extension =
      TestKioskExtensionBuilder(extensions::Manifest::TYPE_PLATFORM_APP, app.id)
          .set_version(app.version)
          .Build();
  std::string manifest_json;
  base::JSONWriter::Write(*extension->manifest()->value(), &manifest_json);
  // Set some generic app information, not necessarily correct. This prevents
  // `KioskAppData` from removing the app.
  // Icon placeholder, as required by `KioskChromeAppManager`.
  constexpr char kFakeIconURL[] = "/chromeos/app_mode/red16x16.png";
  fake_cws.SetAppDetails(app.id, /*localized_name=*/"Test App",
                         /*icon_url=*/kFakeIconURL, manifest_json);
}

}  // namespace

class KioskMultiChromeAppTest : public KioskBaseTest {
 public:
  KioskMultiChromeAppTest() = default;

  KioskMultiChromeAppTest(const KioskMultiChromeAppTest&) = delete;
  KioskMultiChromeAppTest& operator=(const KioskMultiChromeAppTest&) = delete;

  ~KioskMultiChromeAppTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    // For update tests, we cache the app in the PRE part, and then we load it
    // in the test, so we need to both store the apps list on teardown (so that
    // the app manager would accept existing files in its extension cache on the
    // next startup) and copy the list to our stub settings provider as well.
    settings_helper_.CopyStoredValue(kAccountsPrefDeviceLocalAccounts);

    KioskBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    settings_helper_.StoreCachedDeviceSetting(kAccountsPrefDeviceLocalAccounts);
    KioskBaseTest::TearDownOnMainThread();
  }

  void PreCacheApp(const std::string& app_id,
                   const std::string& version,
                   const std::string& crx_file,
                   bool wait_for_app_data) {
    SetTestApp(app_id, version, crx_file);

    KioskChromeAppManager* manager = KioskChromeAppManager::Get();
    TestAppDataLoadWaiter waiter(manager, app_id, version);
    ReloadKioskApps();
    if (wait_for_app_data) {
      waiter.WaitForAppData();
    } else {
      waiter.Wait();
    }
    EXPECT_TRUE(waiter.loaded());

    auto crx_info = manager->GetCachedCrx(app_id);
    ASSERT_TRUE(crx_info.has_value());
    auto& [_, cached_version] = crx_info.value();
    EXPECT_EQ(version, cached_version);
  }

  void LaunchKioskWithSecondaryApps(
      const TestAppInfo& primary_app,
      const std::vector<TestAppInfo>& secondary_apps) {
    // Pre-cache the primary app.
    PreCacheApp(primary_app.id, primary_app.version, primary_app.crx_filename,
                /*wait_for_app_data=*/false);

    fake_cws().SetNoUpdate(primary_app.id);
    for (const auto& app : secondary_apps) {
      fake_cws().SetUpdateCrx(app.id, app.crx_filename, app.version);
    }

    // Launch the primary app.
    SimulateNetworkOnline();
    ASSERT_TRUE(LaunchApp(test_app_id()));
    WaitForAppLaunchWithOptions(false, true);

    // Verify the primary app and the secondary apps are all installed.
    EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), primary_app.id),
              primary_app.version);
    for (const auto& app : secondary_apps) {
      EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), app.id),
                app.version);
      EXPECT_EQ(ManifestType(app.id), app.type);
    }
  }

  void LaunchTestKioskAppWithTwoSecondaryApps() {
    TestAppInfo primary_app{/*id=*/kPrimaryAppId,
                            /*version=*/"1.0.0",
                            /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

    TestAppInfo secondary_app_1{
        /*id=*/kSecondaryApp1Id,
        /*version=*/"1.0.0",
        /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

    TestAppInfo secondary_app_2{
        /*id=*/kSecondaryApp2Id,
        /*version=*/"1.0.0",
        /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

    SetupAppDetailInFakeCws(fake_cws(), primary_app);
    SetupAppDetailInFakeCws(fake_cws(), secondary_app_1);
    SetupAppDetailInFakeCws(fake_cws(), secondary_app_2);

    LaunchKioskWithSecondaryApps(primary_app,
                                 {secondary_app_1, secondary_app_2});
  }

  void LaunchTestKioskAppWithSecondaryExtension() {
    TestAppInfo primary_app{/*id=*/kPrimaryAppId,
                            /*version=*/"24.0.0",
                            /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

    SetupAppDetailInFakeCws(fake_cws(), primary_app);

    LaunchKioskWithSecondaryApps(
        primary_app,
        {TestAppInfo{/*id=*/kSecondaryExtensionId,
                     /*version=*/"1.0.0",
                     /*type=*/extensions::Manifest::TYPE_EXTENSION}});
  }

  void LaunchAppWithSharedModuleAndSecondaryApp() {
    TestAppInfo primary_app{/*id=*/kSharedModulePrimaryAppId,
                            /*version=*/"1.0.0",
                            /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

    TestAppInfo secondary_app{/*id=*/kSecondaryAppId,
                              /*version=*/"1.0.0",
                              /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

    // `FakeCWS` sets up shared modules the same way as secondary apps.
    TestAppInfo shared_module{
        /*id=*/kSharedModuleId,
        /*version=*/"1.0.0",
        /*type=*/extensions::Manifest::TYPE_SHARED_MODULE};

    SetupAppDetailInFakeCws(fake_cws(), primary_app);

    LaunchKioskWithSecondaryApps(primary_app, {secondary_app, shared_module});
    EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), shared_module.id),
              shared_module.version);
  }

  void LaunchAppWithSharedModule() {
    TestAppInfo primary_app{/*id=*/kSharedModulePrimaryAppId,
                            /*version=*/"2.0.0",
                            /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

    SetupAppDetailInFakeCws(fake_cws(), primary_app);

    LaunchKioskWithSecondaryApps(
        primary_app,
        {TestAppInfo{/*id=*/kSharedModuleId,
                     /*version=*/"1.0.0",
                     /*type=*/extensions::Manifest::TYPE_SHARED_MODULE}});
  }

  bool PrimaryAppUpdateIsPending() const {
    Profile* app_profile = ProfileManager::GetPrimaryUserProfile();
    return !!extensions::ExtensionSystem::Get(app_profile)
                 ->extension_service()
                 ->GetPendingExtensionUpdate(test_app_id());
  }

  FakeCWS& fake_cws() { return CHECK_DEREF(KioskBaseTest::fake_cws()); }
};

// Launch a primary kiosk app which has two secondary apps.
IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       LaunchTestKioskAppWithTwoSecondaryApps) {
  LaunchTestKioskAppWithTwoSecondaryApps();
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       PRE_UpdateMultiAppKioskRemoveOneApp) {
  LaunchTestKioskAppWithTwoSecondaryApps();
}

// Update the primary app to version 2 which removes one of the secondary app
// from its manifest.
IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       UpdateMultiAppKioskRemoveOneApp) {
  TestAppInfo primary_app{/*id=*/kPrimaryAppId,
                          /*version=*/"2.0.0",
                          /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

  SetTestApp(primary_app.id);
  fake_cws().SetUpdateCrx(primary_app.id, primary_app.crx_filename,
                          primary_app.version);
  SetupAppDetailInFakeCws(fake_cws(), primary_app);
  fake_cws().SetNoUpdate(kSecondaryApp1Id);
  fake_cws().SetNoUpdate(kSecondaryApp2Id);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the secondary app kSecondaryApp1Id is removed.
  EXPECT_EQ("2.0.0",
            InstalledChromeAppVersion(CurrentProfile(), test_app_id()));
  EXPECT_FALSE(IsChromeAppInstalled(CurrentProfile(), kSecondaryApp1Id));
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), kSecondaryApp2Id),
            "1.0.0");
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       PRE_UpdateMultiAppKioskAddOneApp) {
  LaunchTestKioskAppWithTwoSecondaryApps();
}

// Update the primary app to version 3 which adds a new secondary app in its
// manifest.
IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest, UpdateMultiAppKioskAddOneApp) {
  TestAppInfo primary_app{/*id=*/kPrimaryAppId,
                          /*version=*/"3.0.0",
                          /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

  SetTestApp(primary_app.id);
  fake_cws().SetUpdateCrx(primary_app.id, primary_app.crx_filename,
                          primary_app.version);
  SetupAppDetailInFakeCws(fake_cws(), primary_app);
  fake_cws().SetNoUpdate(kSecondaryApp1Id);
  fake_cws().SetNoUpdate(kSecondaryApp2Id);

  TestAppInfo secondary_app{/*id=*/kSecondaryApp3Id,
                            /*version=*/"1.0.0",
                            /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

  fake_cws().SetUpdateCrx(secondary_app.id, secondary_app.crx_filename,
                          secondary_app.version);
  SetupAppDetailInFakeCws(fake_cws(), secondary_app);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the secondary app kSecondaryApp3Id is installed.
  EXPECT_EQ("3.0.0", GetInstalledAppVersion().GetString());
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), kSecondaryApp1Id),
            "1.0.0");
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), kSecondaryApp2Id),
            "1.0.0");
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), kSecondaryApp3Id),
            "1.0.0");
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       LaunchKioskAppWithSecondaryExtension) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-22a4b826-851a-4065-a32b-273a0e261bf3");

  LaunchTestKioskAppWithSecondaryExtension();
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       LaunchAppWithSharedModuleAndSecondaryApp) {
  LaunchAppWithSharedModuleAndSecondaryApp();
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       PRE_UpdateAppWithSharedModuleRemoveAllSecondaryApps) {
  LaunchAppWithSharedModuleAndSecondaryApp();
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       UpdateAppWithSharedModuleRemoveAllSecondaryApps) {
  SetTestApp(kSharedModulePrimaryAppId);
  fake_cws().SetUpdateCrx(kSharedModulePrimaryAppId,
                          std::string(kSharedModulePrimaryAppId) + "-2.0.0.crx",
                          "2.0.0");
  fake_cws().SetNoUpdate(kSecondaryApp1Id);
  fake_cws().SetNoUpdate(kSharedModuleId);

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the secondary app is removed.
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), kSharedModuleId),
            "1.0.0");
  EXPECT_FALSE(IsChromeAppInstalled(CurrentProfile(), kSecondaryApp1Id));
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       PRE_LaunchAppWithUpdatedModule) {
  LaunchAppWithSharedModule();
  // Verify the shared module is installed with version 1.0.0.
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), kSharedModuleId),
            "1.0.0");
}

// This simulates the case the shared module is updated to a newer version.
// See crbug.com/555083.
IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest, LaunchAppWithUpdatedModule) {
  // No update for primary app, while the shared module is set up to a new
  // version on cws.
  SetTestApp(kSharedModulePrimaryAppId);
  fake_cws().SetNoUpdate(kSharedModulePrimaryAppId);
  fake_cws().SetUpdateCrx(kSharedModuleId,
                          std::string(kSharedModuleId) + "-2.0.0.crx", "2.0.0");

  SimulateNetworkOnline();
  EXPECT_TRUE(LaunchApp(test_app_id()));
  WaitForAppLaunchWithOptions(false, true);

  // Verify the shared module is updated to the new version after primary app
  // is launched.
  EXPECT_EQ(InstalledChromeAppVersion(CurrentProfile(), kSharedModuleId),
            "2.0.0");
}

IN_PROC_BROWSER_TEST_F(KioskMultiChromeAppTest,
                       LaunchAppWithSecondaryArcLikeAppAndExtension) {
  TestAppInfo primary_app{/*id=*/kSharedModulePrimaryAppId,
                          /*version=*/"3.0.0",
                          /*type=*/extensions::Manifest::TYPE_PLATFORM_APP};

  SetupAppDetailInFakeCws(fake_cws(), primary_app);

  LaunchKioskWithSecondaryApps(
      primary_app,
      {TestAppInfo{/*id=*/kSharedModuleId,
                   /*version=*/"1.0.0",
                   /*type=*/extensions::Manifest::TYPE_SHARED_MODULE},
       // The secondary app has a shared module, which is similar to an ARC app.
       TestAppInfo{/*id=*/kSecondaryAppId,
                   /*version=*/"2.0.0",
                   /*type=*/extensions::Manifest::TYPE_PLATFORM_APP},
       TestAppInfo{/*id=*/kSecondaryExtId,
                   /*version=*/"1.0.0",
                   /*type=*/extensions::Manifest::TYPE_EXTENSION}});
}

}  // namespace ash
