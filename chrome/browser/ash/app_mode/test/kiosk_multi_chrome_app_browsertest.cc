// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/strings/strcat.h"
#include "base/test/gtest_tags.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test_kiosk_extension_builder.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/scoped_test_mv2_enabler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::CloseAppWindow;
using kiosk::test::CurrentProfile;
using kiosk::test::InstalledChromeAppVersion;
using kiosk::test::IsChromeAppInstalled;
using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

// Workflow COM_KIOSK_CUJ3_TASK6_WF1.
constexpr std::string_view kSecondaryExtensionTag =
    "screenplay-22a4b826-851a-4065-a32b-273a0e261bf3";

KioskMixin::CwsChromeAppOption PrimaryAppV1() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //     src/primary_app
  constexpr std::string_view kAppId = "fclmjfpgiaifbnbnlpmdjhicolkapihc";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"multi-chrome-app-primary-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, "-1.0.0.crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption PrimaryAppV2() {
  auto app_v2 = PrimaryAppV1();
  app_v2.crx_filename = base::StrCat({app_v2.app_id, "-2.0.0.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption PrimaryAppV3() {
  auto app_v3 = PrimaryAppV1();
  app_v3.crx_filename = base::StrCat({app_v3.app_id, "-3.0.0.crx"});
  app_v3.crx_version = "3.0.0";
  return app_v3;
}

KioskMixin::CwsChromeAppOption PrimaryAppV24() {
  auto app_v24 = PrimaryAppV1();
  app_v24.crx_filename = base::StrCat({app_v24.app_id, "-24.0.0.crx"});
  app_v24.crx_version = "24.0.0";
  return app_v24;
}

KioskMixin::CwsChromeAppOption SharedModulePrimaryAppV1() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //     src/shared_module_primary_app
  constexpr std::string_view kAppId = "kidkeddeanfhailinhfokehpolmfdppa";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"multi-chrome-app-shared-module-primary-app@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, "-1.0.0.crx"}),
      /*crx_version=*/"1.0.0"};
}

KioskMixin::CwsChromeAppOption SharedModulePrimaryAppV2() {
  auto app_v2 = SharedModulePrimaryAppV1();
  app_v2.crx_filename = base::StrCat({app_v2.app_id, "-2.0.0.crx"});
  app_v2.crx_version = "2.0.0";
  return app_v2;
}

KioskMixin::CwsChromeAppOption PrimaryAppWithSecondaryAppAndExtensionV1() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //     src/primary_app_with_secondary_app_and_extension
  constexpr std::string_view kAppId = "meoofpdmandomdeepdejefgcodglecaj";

  return KioskMixin::CwsChromeAppOption{
      /*account_id=*/"multi-chrome-app-with-app-and-extension@localhost",
      /*app_id=*/kAppId,
      /*crx_filename=*/base::StrCat({kAppId, "-1.0.0.crx"}),
      /*crx_version=*/"1.0.0"};
}

// Helper struct to hold information about one of the secondary apps.
struct AppInfo {
  AppInfo(std::string_view id,
          std::string_view version,
          extensions::Manifest::Type type)
      : id(std::string(id)),
        version(std::string(version)),
        crx_filename(base::StrCat({id, "-", version, ".crx"})),
        type(type) {}
  AppInfo(const AppInfo&) = default;
  AppInfo& operator=(const AppInfo&) = default;
  AppInfo(AppInfo&&) = default;
  AppInfo& operator=(AppInfo&&) = default;
  ~AppInfo() = default;

  std::string id;
  std::string version;
  std::string crx_filename;
  extensions::Manifest::Type type;
};

void ServeAppsInCws(FakeCWS& fake_cws, const std::vector<AppInfo>& apps) {
  for (const auto& app : apps) {
    fake_cws.SetUpdateCrx(app.id, app.crx_filename, app.version);
  }
}

AppInfo SecondaryApp1() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //     src/secondary_app_1
  return AppInfo{
      /*id=*/"elbhpkeieolijdlflcplbbabceggjknh",
      /*version=*/"1.0.0",
      /*type=*/extensions::Manifest::TYPE_PLATFORM_APP,
  };
}

AppInfo SecondaryApp2() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //      src/secondary_app_2
  return AppInfo{
      /*id=*/"coamgmmgmjeeaodkbpdajekljacgfhkc",
      /*version=*/"1.0.0",
      /*type=*/extensions::Manifest::TYPE_PLATFORM_APP,
  };
}

AppInfo SecondaryApp3() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //      src/secondary_app_3
  return AppInfo{
      /*id=*/"miccbahcahimnejpdoaafjeolookhoem",
      /*version=*/"1.0.0",
      /*type=*/extensions::Manifest::TYPE_PLATFORM_APP,
  };
}

AppInfo SecondaryExtension1() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //      src/secondary_extensions_1
  return AppInfo{
      /*id=*/"pegeblegnlhnpgghhjblhchdllfijodp",
      /*version=*/"1.0.0",
      /*type=*/extensions::Manifest::TYPE_EXTENSION,
  };
}

AppInfo SharedModuleAppV1() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //      src/shared_module
  return AppInfo{
      /*id=*/"hpanhkopkhnkpcmnedlnjmkfafmlamak",
      /*version=*/"1.0.0",
      /*type=*/extensions::Manifest::TYPE_SHARED_MODULE,
  };
}

AppInfo SharedModuleAppV2() {
  auto app_v1 = SharedModuleAppV1();
  return AppInfo{app_v1.id, /*version=*/"2.0.0", app_v1.type};
}

AppInfo SecondaryAppV1() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //     src/secondary_app
  return AppInfo{
      /*id=*/"ffceghmcpipkneddgikbgoagnheejdbf",
      /*version=*/"1.0.0",
      /*type=*/extensions::Manifest::TYPE_PLATFORM_APP,
  };
}

AppInfo SecondaryAppV2WithSharedModule() {
  auto app_v1 = SecondaryAppV1();
  return AppInfo{app_v1.id, /*version=*/"2.0.0", app_v1.type};
}

AppInfo SecondaryExtension() {
  // Source files are in
  // //chrome/test/data/chromeos/app_mode/apps_and_extensions/multi_app_kiosk/
  //      src/secondary_extension
  return AppInfo{
      /*id=*/"meaknlbicgahoejcchpnkenkmbekcddf",
      /*version=*/"1.0.0",
      /*type=*/extensions::Manifest::TYPE_EXTENSION,
  };
}

extensions::Manifest::Type ManifestType(const extensions::ExtensionId id) {
  const auto& app_or_extension =
      CHECK_DEREF(extensions::ExtensionRegistry::Get(&CurrentProfile())
                      ->GetInstalledExtension(id));
  return app_or_extension.GetType();
}

void ExpectAppsAreInstalled(Profile& profile,
                            const KioskApp& kiosk_app,
                            const KioskMixin::CwsChromeAppOption& primary_app,
                            const std::vector<AppInfo>& secondary_apps) {
  EXPECT_EQ(kiosk_app.id().app_id.value(), primary_app.app_id);
  EXPECT_EQ(InstalledChromeAppVersion(profile, kiosk_app),
            primary_app.crx_version);
  for (const auto& app : secondary_apps) {
    EXPECT_EQ(InstalledChromeAppVersion(profile, app.id), app.version);
    EXPECT_EQ(ManifestType(app.id), app.type);
  }
}

// Parameter data used in `KioskMultiChromeAppTest`.
struct TestParam {
  TestParam(std::string_view name,
            KioskMixin::CwsChromeAppOption pre_primary_app,
            std::vector<AppInfo> pre_secondary_apps,
            KioskMixin::CwsChromeAppOption primary_app,
            std::vector<AppInfo> secondary_apps,
            std::string_view feature_tag = "")
      : name(std::string(name)),
        pre_primary_app(std::move(pre_primary_app)),
        pre_secondary_apps(std::move(pre_secondary_apps)),
        primary_app(std::move(primary_app)),
        secondary_apps(std::move(secondary_apps)),
        feature_tag(feature_tag) {}
  TestParam(std::string_view name,
            KioskMixin::CwsChromeAppOption primary_app,
            std::vector<AppInfo> secondary_apps,
            std::string_view feature_tag = "")
      : TestParam(name,
                  /*pre_primary_app=*/primary_app,
                  /*pre_secondary_apps=*/{},
                  std::move(primary_app),
                  std::move(secondary_apps),
                  feature_tag) {}
  TestParam(const TestParam&) = default;
  TestParam(TestParam&&) = default;
  ~TestParam() = default;

  //  The name of this test parameter to be set in gtest.
  std::string name;
  //  The primary Kiosk app to be used in the PRE_ test.
  KioskMixin::CwsChromeAppOption pre_primary_app;
  //  The secondary apps to be used in the PRE_ test.
  std::vector<AppInfo> pre_secondary_apps;
  //  The primary Kiosk app to be used in the (not PRE_) test.
  KioskMixin::CwsChromeAppOption primary_app;
  //  The secondary apps to be used in the (not PRE_) test.
  std::vector<AppInfo> secondary_apps;
  // The feature ID tag of this tests, if any. Can be empty.
  std::string_view feature_tag;
};

std::string TestParamName(const testing::TestParamInfo<TestParam>& info) {
  return info.param.name;
}

void AddFeatureTag(std::string_view feature_tag) {
  if (feature_tag.empty()) {
    return;
  }
  base::AddFeatureIdTagToTestResult(std::string(feature_tag));
}

}  // namespace

// Verifies Chrome apps with secondary apps and shared modules work in Kiosk.
class KioskMultiChromeAppTest : public MixinBasedInProcessBrowserTest,
                                public testing::WithParamInterface<TestParam> {
 public:
  KioskMultiChromeAppTest() = default;
  KioskMultiChromeAppTest(const KioskMultiChromeAppTest&) = delete;
  KioskMultiChromeAppTest& operator=(const KioskMultiChromeAppTest&) = delete;
  ~KioskMultiChromeAppTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ServeAppsInCws(kiosk_.fake_cws(), CurrentSecondaryApps());

    ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
    EXPECT_TRUE(WaitKioskLaunched());
  }

  const KioskMixin::CwsChromeAppOption& CurrentPrimaryApp() {
    return GetTestPreCount() == 1 ? GetParam().pre_primary_app
                                  : GetParam().primary_app;
  }

  const std::vector<AppInfo>& CurrentSecondaryApps() {
    return GetTestPreCount() == 1 ? GetParam().pre_secondary_apps
                                  : GetParam().secondary_apps;
  }

  std::vector<AppInfo> AppsRemovedAfterUpdate() {
    const auto& pre_update_apps = GetParam().pre_secondary_apps;
    const auto& post_update_apps = GetParam().secondary_apps;
    // Copy all `pre_update_apps` that are not in `post_update_apps`.
    std::vector<AppInfo> result;
    std::ranges::copy_if(pre_update_apps, std::back_inserter(result),
                         [&post_update_apps](const auto& pre_update_app) {
                           return std::ranges::none_of(
                               post_update_apps,
                               [&pre_update_app](const auto& post_update_app) {
                                 return pre_update_app.id == post_update_app.id;
                               });
                         });
    return result;
  }

  KioskMixin kiosk_{
      &mixin_host_,
      /*cached_configuration=*/KioskMixin::Config{/*name=*/{},
                                                  {},
                                                  {CurrentPrimaryApp()}}};

 private:
  // TODO(https://crbug.com/40804030): Remove this when updated to use MV3.
  extensions::ScopedTestMV2Enabler mv2_enabler_;
};

IN_PROC_BROWSER_TEST_P(KioskMultiChromeAppTest, InstallsAndLaunchesMultiApp) {
  AddFeatureTag(GetParam().feature_tag);
  ExpectAppsAreInstalled(CurrentProfile(), TheKioskApp(), CurrentPrimaryApp(),
                         CurrentSecondaryApps());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskMultiChromeAppTest,
    testing::Values(
        TestParam{
            /*name=*/"ChromeAppWithTwoSecondaryApps",
            /*primary_app=*/PrimaryAppV1(),
            /*secondary_apps=*/{SecondaryApp1(), SecondaryApp2()},
        },
        TestParam{
            /*name=*/"ChromeAppWithSecondaryExtension",
            /*primary_app=*/PrimaryAppV24(),
            /*secondary_apps=*/{SecondaryExtension1()},
            /*feature_tag=*/kSecondaryExtensionTag,
        },
        TestParam{
            /*name=*/"SharedModuleChromeAppWithSecondaryApp",
            /*primary_app=*/SharedModulePrimaryAppV1(),
            /*secondary_apps=*/{SharedModuleAppV1(), SecondaryAppV1()},
        },
        TestParam{
            /*name=*/"ChromeAppWithSecondarySharedModuleAppAndExtension",
            /*primary_app=*/PrimaryAppWithSecondaryAppAndExtensionV1(),
            /*secondary_apps=*/
            {
                SharedModuleAppV1(),
                SecondaryAppV2WithSharedModule(),
                SecondaryExtension(),
            },
        }),
    TestParamName);

// Verifies Chrome app updates adding or removing secondary apps and shared
// modules work in Kiosk.
using KioskMultiChromeAppUpdateTest = KioskMultiChromeAppTest;

IN_PROC_BROWSER_TEST_P(KioskMultiChromeAppUpdateTest, PRE_UpdatesMultiApp) {
  auto& profile = CurrentProfile();

  ExpectAppsAreInstalled(profile, TheKioskApp(), CurrentPrimaryApp(),
                         CurrentSecondaryApps());
}

IN_PROC_BROWSER_TEST_P(KioskMultiChromeAppUpdateTest, UpdatesMultiApp) {
  auto& profile = CurrentProfile();

  ExpectAppsAreInstalled(profile, TheKioskApp(), CurrentPrimaryApp(),
                         CurrentSecondaryApps());

  for (const auto& removed_app : AppsRemovedAfterUpdate()) {
    EXPECT_FALSE(IsChromeAppInstalled(profile, removed_app.id));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskMultiChromeAppUpdateTest,
    testing::Values(
        TestParam{
            /*name=*/"RemovesOneSecondaryApp",
            /*pre_primary_app=*/PrimaryAppV1(),
            /*pre_secondary_apps=*/{SecondaryApp1(), SecondaryApp2()},
            /*primary_app=*/PrimaryAppV2(),
            /*secondary_apps=*/{SecondaryApp2()},
        },
        TestParam{
            /*name=*/"AddsOneApp",  // AddsOneSecondaryApp
            /*pre_primary_app=*/PrimaryAppV1(),
            /*pre_secondary_apps=*/{SecondaryApp1(), SecondaryApp2()},
            /*primary_app=*/PrimaryAppV3(),
            /*secondary_apps=*/
            {SecondaryApp1(), SecondaryApp2(), SecondaryApp3()},
        },
        TestParam{
            /*name=*/"RemovesSecondaryAppsButKeepsOneSharedModule",
            /*pre_primary_app=*/SharedModulePrimaryAppV1(),
            /*pre_secondary_apps=*/
            {SecondaryAppV1(), SharedModuleAppV1()},
            /*primary_app=*/SharedModulePrimaryAppV2(),
            /*secondary_apps=*/{SharedModuleAppV1()},
        },
        TestParam{
            /*name=*/"UpdatesSharedModule",
            /*pre_primary_app=*/SharedModulePrimaryAppV1(),
            /*pre_secondary_apps=*/
            {SecondaryAppV1(), SharedModuleAppV1()},
            /*primary_app=*/SharedModulePrimaryAppV1(),
            /*secondary_apps=*/{SecondaryAppV1(), SharedModuleAppV2()},
        }),
    TestParamName);

}  // namespace ash
