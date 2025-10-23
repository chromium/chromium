// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>
#include <vector>

#include "ash/public/cpp/login_accelerators.h"
#include "base/check_deref.h"
#include "base/task/current_thread.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/features.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

const web_package::SignedWebBundleId kTestWebBundleId =
    web_app::test::GetDefaultEd25519WebBundleId();

KioskMixin::Config GetKioskIwaManualLaunchConfig(
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/"simple-iwa@localhost",
      /*web_bundle_id=*/kTestWebBundleId,
      /*update_manifest_url=*/update_manifest_url);
  iwa_option.skip_iwa_allowlist_checks = false;

  KioskMixin::Config kiosk_iwa_config = {
      /*name=*/"IsolatedWebApp",
      /*auto_launch_account_id=*/std::nullopt,
      {iwa_option}};
  return kiosk_iwa_config;
}

}  // anonymous namespace

using ash::KioskAppLaunchError;
using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

class KioskIwaAllowlistTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaAllowlistTest() {
    iwa_test_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(web_app::test::GetDefaultEd25519KeyPair()));
  }

  ~KioskIwaAllowlistTest() override = default;
  KioskIwaAllowlistTest(const KioskIwaAllowlistTest&) = delete;
  KioskIwaAllowlistTest& operator=(const KioskIwaAllowlistTest&) = delete;

  void WaitForIwaNotAllowedLaunchError() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return KioskAppLaunchError::Get(
                 CHECK_DEREF(g_browser_process->local_state())) ==
             KioskAppLaunchError::Error::kIsolatedAppNotAllowed;
    }));
  }

  bool PressBailoutAccelerator() {
    return ash::LoginDisplayHost::default_host()->HandleAccelerator(
        ash::LoginAcceleratorAction::kAppLaunchBailout);
  }

  void SetIwaAllowlist(
      const std::vector<web_package::SignedWebBundleId>& managed_allowlist) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_THAT(web_app::test::UpdateKeyDistributionInfoWithAllowlist(
                    base::Version("1.0"), std::move(managed_allowlist)),
                base::test::HasValue());
  }

 protected:
  web_app::IsolatedWebAppTestUpdateServer iwa_test_server_;
  KioskMixin kiosk_{
      &mixin_host_,
      GetKioskIwaManualLaunchConfig(
          iwa_test_server_.GetUpdateManifestUrl(kTestWebBundleId))};
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kIsolatedWebAppManagedAllowlist};
};

IN_PROC_BROWSER_TEST_F(KioskIwaAllowlistTest,
                       AllowlistedAppInstalledAndLaunched) {
  SetIwaAllowlist({kTestWebBundleId});

  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_F(KioskIwaAllowlistTest,
                       NotAllowlistedAppInstallationError) {
  // Clear the allowlist
  SetIwaAllowlist(/*managed_allowlist=*/{});

  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  WaitForIwaNotAllowedLaunchError();

  EXPECT_TRUE(ash::KioskController::Get().IsSessionStarting());
  PressBailoutAccelerator();
  RunUntilBrowserProcessQuits();
  EXPECT_EQ(
      KioskAppLaunchError::Get(CHECK_DEREF(g_browser_process->local_state())),
      KioskAppLaunchError::Error::kUserCancel);
  EXPECT_FALSE(ash::KioskController::Get().IsSessionStarting());
}

}  // namespace ash
