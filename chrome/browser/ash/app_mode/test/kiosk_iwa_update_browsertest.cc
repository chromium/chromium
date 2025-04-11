// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

using kiosk::test::BlockKioskLaunch;
using kiosk::test::CurrentProfile;
using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

using UpdateApplyTaskFuture = base::test::TestFuture<
    web_app::IsolatedWebAppUpdateApplyTask::CompletionStatus>;

namespace {

constexpr char kTestAccountId[] = "simple-iwa@localhost";

constexpr char kTestIwaTitle1[] = "First app title";
constexpr char kTestIwaVersion1[] = "0.1";

constexpr char kTestIwaTitle2[] = "Changed title";
constexpr char kTestIwaVersion2[] = "0.2";

web_package::test::KeyPair GetTestKeyPair() {
  return web_app::test::GetDefaultEcdsaP256KeyPair();
}

web_package::SignedWebBundleId GetTestWebBundleId() {
  return web_app::test::GetDefaultEcdsaP256WebBundleId();
}

webapps::AppId GetTestWebAppId() {
  return web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
             GetTestWebBundleId())
      .app_id();
}

web_app::WebAppProvider* GetWebAppProviderPtr() {
  return web_app::WebAppProvider::GetForWebApps(&CurrentProfile());
}

web_app::WebAppProvider& GetWebAppProvider() {
  return CHECK_DEREF(GetWebAppProviderPtr());
}

void WaitForWebAppProvider() {
  test::TestPredicateWaiter(base::BindRepeating([]() {
    return GetWebAppProviderPtr() != nullptr;
  })).Wait();
}

const web_app::WebApp& GetIsolatedWebApp(const webapps::AppId& app_id) {
  return CHECK_DEREF(GetWebAppProvider().registrar_unsafe().GetAppById(app_id));
}

web_app::IsolatedWebAppUpdateApplyTask::CompletionStatus
WaitForTestAppUpdate() {
  UpdateApplyTaskFuture apply_update_future;
  web_app::UpdateApplyTaskResultWaiter apply_update_waiter(
      GetWebAppProvider(), GetTestWebAppId(),
      apply_update_future.GetCallback());
  return apply_update_future.Take();
}

KioskMixin::Config GetKioskIwaManualLaunchConfig(
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/kTestAccountId,
      /*web_bundle_id=*/GetTestWebBundleId(),
      /*update_manifest_url=*/update_manifest_url);

  KioskMixin::Config kiosk_iwa_config = {/*name=*/"IsolatedWebApp",
                                         /*auto_launch_account_id=*/{},
                                         {iwa_option}};
  return kiosk_iwa_config;
}

}  // namespace

class KioskIwaUpdateTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaUpdateTest() { AddBundle(kTestIwaTitle1, kTestIwaVersion1); }

  ~KioskIwaUpdateTest() override = default;
  KioskIwaUpdateTest(const KioskIwaUpdateTest&) = delete;
  KioskIwaUpdateTest& operator=(const KioskIwaUpdateTest&) = delete;

  void AddBundle(std::string_view app_name, std::string_view app_version) {
    iwa_server_mixin_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetName(app_name).SetVersion(
                app_version))
            .BuildBundle(GetTestKeyPair()));
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      ash::features::kIsolatedWebAppKiosk};
  web_app::IsolatedWebAppUpdateServerMixin iwa_server_mixin_{&mixin_host_};
  KioskMixin kiosk_{
      &mixin_host_,
      GetKioskIwaManualLaunchConfig(
          iwa_server_mixin_.GetUpdateManifestUrl(GetTestWebBundleId()))};
};

IN_PROC_BROWSER_TEST_F(KioskIwaUpdateTest, PRE_UpdatesToLatest) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(GetIsolatedWebApp(GetTestWebAppId()).isolation_data()->version(),
            base::Version(kTestIwaVersion1));
}

IN_PROC_BROWSER_TEST_F(KioskIwaUpdateTest, UpdatesToLatest) {
  EXPECT_EQ(TheKioskApp().name(), kTestIwaTitle1);

  AddBundle(kTestIwaTitle2, kTestIwaVersion2);

  // Prevents the app launch to let the update apply.
  auto scoped_launch_blocker = BlockKioskLaunch();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitForWebAppProvider();
  EXPECT_TRUE(WaitForTestAppUpdate().has_value());

  scoped_launch_blocker.reset();
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_EQ(GetIsolatedWebApp(GetTestWebAppId()).isolation_data()->version(),
            base::Version(kTestIwaVersion2));
  EXPECT_EQ(TheKioskApp().name(), kTestIwaTitle2);
}

}  // namespace ash
