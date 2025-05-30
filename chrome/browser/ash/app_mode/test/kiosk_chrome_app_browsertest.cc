// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "base/check_deref.h"
#include "chrome/browser/apps/app_service/publishers/chrome_app_deprecation.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/mojom/manifest.mojom-data-view.h"
#include "extensions/components/native_app_window/native_app_window_views.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

using kiosk::test::LaunchAppManually;
using kiosk::test::WaitKioskLaunched;

namespace {

using extensions::mojom::ManifestLocation;
using kiosk::test::CurrentProfile;
using kiosk::test::TheKioskChromeApp;

ManifestLocation InstallationSource(Profile& profile, std::string_view app_id) {
  auto& chrome_app = CHECK_DEREF(
      extensions::ExtensionRegistry::Get(&profile)->GetInstalledExtension(
          std::string(app_id)));
  return chrome_app.location();
}

KioskChromeAppManager::App GetAppFromManager(const KioskApp& app) {
  return KioskChromeAppManager::Get()->GetApp(app.id().app_id.value()).value();
}

}  // namespace

// Verifies generic Chrome app features in Kiosk.
class KioskChromeAppTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskChromeAppTest() = default;
  KioskChromeAppTest(const KioskChromeAppTest&) = delete;
  KioskChromeAppTest& operator=(const KioskChromeAppTest&) = delete;
  ~KioskChromeAppTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/KioskMixin::Config{
                        /*name=*/{},
                        KioskMixin::AutoLaunchAccount{
                            KioskMixin::SimpleChromeAppOption().account_id},
                        {KioskMixin::SimpleChromeAppOption()}}};
};

IN_PROC_BROWSER_TEST_F(KioskChromeAppTest, InstallsAppFromPolicy) {
  EXPECT_EQ(ManifestLocation::kExternalPolicy,
            InstallationSource(CurrentProfile(),
                               TheKioskChromeApp().id().app_id.value()));
}

// Covers crbug.com/1235334.
IN_PROC_BROWSER_TEST_F(KioskChromeAppTest, AppWindowIsFullScreen) {
  auto& registry =
      CHECK_DEREF(extensions::AppWindowRegistry::Get(&CurrentProfile()));
  auto& window = CHECK_DEREF(
      apps::AppWindowWaiter(&registry, TheKioskChromeApp().id().app_id.value())
          .Wait());
  auto& views =
      CHECK_DEREF(static_cast<native_app_window::NativeAppWindowViews*>(
          window.GetBaseWindow()));

  // `frame_view` and `client_view` should have the same bounds.
  auto& non_client_view = CHECK_DEREF(views.widget()->non_client_view());
  auto& frame_view = CHECK_DEREF(non_client_view.frame_view());
  auto& client_view = CHECK_DEREF(non_client_view.client_view());
  EXPECT_EQ(frame_view.bounds(), client_view.bounds());
}

// Verifies the `was_auto_launched_with_zero_delay` flag is set correctly.
class KioskAutoLaunchWithZeroDelayTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskAutoLaunchWithZeroDelayTest() = default;
  KioskAutoLaunchWithZeroDelayTest(const KioskAutoLaunchWithZeroDelayTest&) =
      delete;
  KioskAutoLaunchWithZeroDelayTest& operator=(
      const KioskAutoLaunchWithZeroDelayTest&) = delete;
  ~KioskAutoLaunchWithZeroDelayTest() override = default;

  bool HasAutoLaunchApp() {
    return GetParam().auto_launch_account_id.has_value();
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
};

IN_PROC_BROWSER_TEST_P(KioskAutoLaunchWithZeroDelayTest, SetsFlagCorrectly) {
  if (!HasAutoLaunchApp()) {
    ASSERT_TRUE(LaunchAppManually(TheKioskChromeApp()));
  }
  ASSERT_TRUE(WaitKioskLaunched());

  auto app = GetAppFromManager(TheKioskChromeApp());
  EXPECT_EQ(app.was_auto_launched_with_zero_delay, HasAutoLaunchApp());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskAutoLaunchWithZeroDelayTest,
    testing::Values(KioskMixin::Config{
                        /*name=*/"AutoLaunch",
                        KioskMixin::AutoLaunchAccount{
                            KioskMixin::SimpleChromeAppOption().account_id},
                        {KioskMixin::SimpleChromeAppOption()}},
                    KioskMixin::Config{/*name=*/"ManualLaunch",
                                       /*auto_launch_account_id=*/{},
                                       {KioskMixin::SimpleChromeAppOption()}}),
    KioskMixin::ConfigName);

}  // namespace ash
