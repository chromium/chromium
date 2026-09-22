// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/check_deref.h"
#include "base/task/current_thread.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/fake_iwa_runtime_data_provider_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::kiosk::test {

namespace {

using web_app::test::GetDefaultEcdsaP256KeyPair;
using web_app::test::GetDefaultEcdsaP256WebBundleId;
using web_app::test::GetDefaultEd25519KeyPair;
using web_app::test::GetDefaultEd25519WebBundleId;

constexpr std::string_view kAccountId1 = "first-iwa@localhost";
constexpr std::string_view kAccountId2 = "second-iwa@localhost";

KioskMixin::Config GetKioskIwaConfig(const GURL& update_manifest_url,
                                     bool auto_launch = true) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/kAccountId1,
      /*web_bundle_id=*/GetDefaultEd25519WebBundleId(),
      /*update_manifest_url=*/update_manifest_url);

  KioskMixin::Config kiosk_iwa_config = {
      /*name=*/"IsolatedWebApp",
      /*auto_launch_account_id=*/
      auto_launch ? std::make_optional(
                        KioskMixin::AutoLaunchAccount(iwa_option.account_id))
                  : std::nullopt,
      {iwa_option}};
  return kiosk_iwa_config;
}

KioskMixin::Config GetKioskIwaManualLaunchConfig(
    const GURL& update_manifest_url) {
  return GetKioskIwaConfig(update_manifest_url, /*auto_launch=*/false);
}

KioskMixin::Config GetKioskIwaAutoLaunchConfig(
    const GURL& update_manifest_url) {
  return GetKioskIwaConfig(update_manifest_url, /*auto_launch=*/true);
}

KioskMixin::Config GetKioskIwaTwoAppsManualLaunchConfig(
    const GURL& update_manifest_url1,
    const GURL& update_manifest_url2) {
  KioskMixin::IsolatedWebAppOption iwa_option1(
      /*account_id=*/kAccountId1,
      /*web_bundle_id=*/GetDefaultEd25519WebBundleId(),
      /*update_manifest_url=*/update_manifest_url1);

  KioskMixin::IsolatedWebAppOption iwa_option2(
      /*account_id=*/kAccountId2,
      /*web_bundle_id=*/GetDefaultEcdsaP256WebBundleId(),
      /*update_manifest_url=*/update_manifest_url2);

  KioskMixin::Config kiosk_iwa_config = {
      /*name=*/"IsolatedWebApp",
      /*auto_launch_account_id=*/std::nullopt,
      {iwa_option1, iwa_option2}};
  return kiosk_iwa_config;
}

KioskApp GetKioskAppByBundleId(web_package::SignedWebBundleId bundle_id) {
  for (KioskApp& app : KioskController::Get().GetApps()) {
    if (auto* kiosk_iwa_data =
            CHECK_DEREF(KioskIwaManager::Get()).GetApp(app.id().account_id);
        kiosk_iwa_data && kiosk_iwa_data->web_bundle_id() == bundle_id) {
      return app;
    }
  }
  NOTREACHED();
}

void WaitForIwaNotAllowedLaunchError() {
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return KioskAppLaunchError::Get(
               CHECK_DEREF(g_browser_process->local_state())) ==
           KioskAppLaunchError::Error::kIsolatedAppNotAllowed;
  }));
}

}  // anonymous namespace

using ash::KioskAppLaunchError;
using kiosk::test::LaunchAppManually;
using kiosk::test::PressBailoutAccelerator;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;
using testing::Contains;
using testing::Eq;
using testing::Field;
using testing::Not;

class KioskIwaAllowlistBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaAllowlistBrowserTest() {
    iwa_test_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(GetDefaultEd25519KeyPair()));
  }
  ~KioskIwaAllowlistBrowserTest() override = default;
  KioskIwaAllowlistBrowserTest(const KioskIwaAllowlistBrowserTest&) = delete;
  KioskIwaAllowlistBrowserTest& operator=(const KioskIwaAllowlistBrowserTest&) =
      delete;

  void SetIwaAllowlist(
      const std::vector<web_package::SignedWebBundleId>& managed_allowlist) {
    EXPECT_OK(
        web_app::test::KeyDistributionComponentBuilder(base::Version("1.0"))
            .WithManagedAllowlist(managed_allowlist)
            .Build()
            .UploadFromComponentFolder());
  }

 protected:
  web_app::IsolatedWebAppTestUpdateServer iwa_test_server_;
  KioskMixin kiosk_{&mixin_host_, GetKioskIwaManualLaunchConfig(
                                      iwa_test_server_.GetUpdateManifestUrl(
                                          GetDefaultEd25519WebBundleId()))};
};

IN_PROC_BROWSER_TEST_F(KioskIwaAllowlistBrowserTest,
                       AllowlistedAppInstalledAndLaunched) {
  SetIwaAllowlist({GetDefaultEd25519WebBundleId()});

  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
}

IN_PROC_BROWSER_TEST_F(KioskIwaAllowlistBrowserTest,
                       NotAllowlistedAppInstallationError) {
  // Clear the allowlist
  SetIwaAllowlist(/*managed_allowlist=*/{});

  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  WaitForIwaNotAllowedLaunchError();

  const auto& local_state = CHECK_DEREF(g_browser_process->local_state());
  EXPECT_EQ(KioskAppLaunchError::Get(local_state),
            KioskAppLaunchError::Error::kIsolatedAppNotAllowed);
  EXPECT_FALSE(KioskAppLaunchError::DidUserCancelLaunch(local_state));
  EXPECT_TRUE(ash::KioskController::Get().IsSessionStarting());

  ASSERT_TRUE(PressBailoutAccelerator());
  RunUntilBrowserProcessQuits();

  EXPECT_EQ(KioskAppLaunchError::Get(local_state),
            KioskAppLaunchError::Error::kIsolatedAppNotAllowed);
  EXPECT_TRUE(KioskAppLaunchError::DidUserCancelLaunch(local_state));
  EXPECT_FALSE(ash::KioskController::Get().IsSessionStarting());
}

class KioskIwaBlocklistBrowserTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaBlocklistBrowserTest() {
    iwa_test_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(GetDefaultEd25519KeyPair()));
    iwa_test_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion("10.0.0"))
            .BuildBundle(GetDefaultEcdsaP256KeyPair()));
  }
  ~KioskIwaBlocklistBrowserTest() override = default;
  KioskIwaBlocklistBrowserTest(const KioskIwaBlocklistBrowserTest&) = delete;
  KioskIwaBlocklistBrowserTest& operator=(const KioskIwaBlocklistBrowserTest&) =
      delete;

 protected:
  web_app::IsolatedWebAppTestUpdateServer iwa_test_server_;
  KioskMixin kiosk_{
      &mixin_host_,
      GetKioskIwaTwoAppsManualLaunchConfig(
          iwa_test_server_.GetUpdateManifestUrl(GetDefaultEd25519WebBundleId()),
          iwa_test_server_.GetUpdateManifestUrl(
              GetDefaultEcdsaP256WebBundleId()))};
};

IN_PROC_BROWSER_TEST_F(KioskIwaBlocklistBrowserTest,
                       AppShutdownOnBlocklisting) {
  EXPECT_OK(web_app::test::KeyDistributionComponentBuilder(base::Version("1.0"))
                .WithManagedAllowlist({GetDefaultEd25519WebBundleId()})
                .Build()
                .UploadFromComponentFolder());
  ASSERT_TRUE(
      LaunchAppManually(GetKioskAppByBundleId(GetDefaultEd25519WebBundleId())));
  ASSERT_TRUE(WaitKioskLaunched());

  EXPECT_OK(web_app::test::KeyDistributionComponentBuilder(base::Version("1.1"))
                .WithBlocklist({GetDefaultEd25519WebBundleId()})
                .Build()
                .UploadFromComponentFolder());

  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return session.is_shutting_down(); }));
}

IN_PROC_BROWSER_TEST_F(KioskIwaBlocklistBrowserTest,
                       SecondAppNotAffectedByBlocklisting) {
  EXPECT_OK(web_app::test::KeyDistributionComponentBuilder(base::Version("1.0"))
                .WithManagedAllowlist({GetDefaultEd25519WebBundleId(),
                                       GetDefaultEcdsaP256WebBundleId()})
                .Build()
                .UploadFromComponentFolder());
  ASSERT_TRUE(
      LaunchAppManually(GetKioskAppByBundleId(GetDefaultEd25519WebBundleId())));
  ASSERT_TRUE(WaitKioskLaunched());

  KioskAppManagerBase::AppList apps_before =
      CHECK_DEREF(ash::KioskIwaManager::Get()).GetApps();
  EXPECT_THAT(apps_before, testing::SizeIs(Eq(2)));

  EXPECT_OK(web_app::test::KeyDistributionComponentBuilder(base::Version("1.1"))
                .WithManagedAllowlist({GetDefaultEd25519WebBundleId()})
                .WithBlocklist({GetDefaultEcdsaP256WebBundleId()})
                .Build()
                .UploadFromComponentFolder());

  auto app_id_1 = web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
                      GetDefaultEd25519WebBundleId())
                      .app_id();

  // Only one app should be removed
  KioskAppManagerBase::AppList apps_after =
      CHECK_DEREF(ash::KioskIwaManager::Get()).GetApps();
  EXPECT_THAT(apps_after, testing::SizeIs(Eq(1)));
  EXPECT_THAT(apps_after,
              Contains(Field(&KioskAppManagerBase::App::app_id, Eq(app_id_1))));
}

class KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest() {
    iwa_test_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(GetDefaultEd25519KeyPair()));
    data_provider_->Update([&](auto& update) {
      update.AddToManagedAllowlist(GetDefaultEd25519WebBundleId());
    });
  }
  ~KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest() override = default;
  KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest(
      const KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest&) = delete;
  KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest& operator=(
      const KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest&) = delete;

 protected:
  web_app::IsolatedWebAppTestUpdateServer iwa_test_server_;
  web_app::FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
  KioskMixin kiosk_{&mixin_host_, GetKioskIwaAutoLaunchConfig(
                                      iwa_test_server_.GetUpdateManifestUrl(
                                          GetDefaultEd25519WebBundleId()))};
};

IN_PROC_BROWSER_TEST_F(KioskIwaBlocklistAutoLaunchInitiallyAllowedBrowserTest,
                       AppShutdownOnBlocklisting) {
  ASSERT_TRUE(WaitKioskLaunched());

  data_provider_->Update([&](auto& update) {
    update.AddToBlocklist(GetDefaultEd25519WebBundleId());
  });

  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return session.is_shutting_down(); }));
}

class KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest() {
    iwa_test_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion("1.0.0"))
            .BuildBundle(GetDefaultEd25519KeyPair()));
    data_provider_->Update([&](auto& update) {
      update.AddToBlocklist(GetDefaultEd25519WebBundleId());
    });
  }
  ~KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest() override = default;
  KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest(
      const KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest&) = delete;
  KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest& operator=(
      const KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest&) = delete;

 protected:
  web_app::IsolatedWebAppTestUpdateServer iwa_test_server_;
  web_app::FakeIwaRuntimeDataProviderMixin data_provider_{&mixin_host_};
  KioskMixin kiosk_{&mixin_host_, GetKioskIwaAutoLaunchConfig(
                                      iwa_test_server_.GetUpdateManifestUrl(
                                          GetDefaultEd25519WebBundleId()))};
};

IN_PROC_BROWSER_TEST_F(KioskIwaBlocklistAutoLaunchInitiallyBlockedBrowserTest,
                       BlocklistedAppFailsToLaunch) {
  WaitForIwaNotAllowedLaunchError();
  // Screen still shows error splash screen
  EXPECT_TRUE(ash::KioskController::Get().IsSessionStarting());
}

}  // namespace ash::kiosk::test
