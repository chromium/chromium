// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/version.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_server_mixin.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/update_channel.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

using kiosk::test::CurrentProfile;
using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

constexpr char kTestAccountId[] = "kiosk-iwa-test@localhost";

constexpr char kTestIwaVersion1[] = "1";
constexpr char kTestIwaVersion2[] = "2.0";
constexpr char kTestIwaVersion3[] = "3.0.0";

constexpr char kTestIwaVersionInvalid1[] = "not a version number";
constexpr char kTestIwaVersionInvalid2[] = "2,0";

constexpr char kChannelNameDefault[] = "default";
constexpr char kChannelNameBeta[] = "beta";
constexpr char kChannelNameAlpha[] = "alpha";
constexpr char kChannelNameUnknown[] = "unknown";

constexpr char kUnsetPolicyValue[] = "";

const web_app::UpdateChannel kChannelBeta =
    web_app::UpdateChannel::Create(kChannelNameBeta).value();
const web_app::UpdateChannel kChannelAlpha =
    web_app::UpdateChannel::Create(kChannelNameAlpha).value();

constexpr std::string_view GetTestAccountId() {
  return kTestAccountId;
}

web_package::SignedWebBundleId GetTestWebBundleId() {
  return web_app::test::GetDefaultEd25519WebBundleId();
}

web_package::test::KeyPair GetTestKeyPair() {
  return web_app::test::GetDefaultEd25519KeyPair();
}

webapps::AppId GetTestWebAppId() {
  return web_app::IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
             GetTestWebBundleId())
      .app_id();
}

web_app::WebAppProvider& GetWebAppProvider() {
  return CHECK_DEREF(web_app::WebAppProvider::GetForWebApps(&CurrentProfile()));
}

const web_app::WebApp& GetIsolatedWebApp(const webapps::AppId& app_id) {
  return CHECK_DEREF(GetWebAppProvider().registrar_unsafe().GetAppById(app_id));
}

// Creates a manual launch IWA kiosk with a custom channel.
KioskMixin::Config CreateManualLaunchConfigWithChannel(
    const std::string& update_channel,
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/GetTestAccountId(),
      /*web_bundle_id=*/GetTestWebBundleId(), update_manifest_url,
      update_channel);

  return {
      /*name=*/"IsolatedWebApp", /*auto_launch_account_id=*/{}, {iwa_option}};
}

// Creates a manual launch IWA kiosk with version pinning.
KioskMixin::Config CreateManualLaunchConfigWithVersionPinning(
    const std::string& pinned_version,
    bool allow_downgrades,
    const GURL& update_manifest_url) {
  KioskMixin::IsolatedWebAppOption iwa_option(
      /*account_id=*/GetTestAccountId(),
      /*web_bundle_id=*/GetTestWebBundleId(), update_manifest_url,
      /*update_channel=*/kUnsetPolicyValue, pinned_version, allow_downgrades);

  KioskMixin::Config kiosk_iwa_config = {/*name=*/"IsolatedWebApp",
                                         /*auto_launch_account_id=*/{},
                                         {iwa_option}};
  return kiosk_iwa_config;
}

}  // namespace

// Base class for Kiosk IWA version management fixtures.
class KioskIwaVersionManagementBaseTest
    : public MixinBasedInProcessBrowserTest {
 public:
  // Factory method to create kiosk mixin configurations based on the url from
  // IsolatedWebAppUpdateServerMixin.
  using ConfigCreator =
      base::OnceCallback<KioskMixin::Config(const GURL& update_manifest_url)>;

  explicit KioskIwaVersionManagementBaseTest(ConfigCreator config_creator)
      : feature_list_(ash::features::kIsolatedWebAppKiosk),
        iwa_server_mixin_(&mixin_host_),
        kiosk_mixin_(&mixin_host_,
                     std::move(config_creator).Run(GetUpdateManifestUrl())) {}

  ~KioskIwaVersionManagementBaseTest() override = default;
  KioskIwaVersionManagementBaseTest(const KioskIwaVersionManagementBaseTest&) =
      delete;
  KioskIwaVersionManagementBaseTest& operator=(
      const KioskIwaVersionManagementBaseTest&) = delete;

  void AddTestBundle(std::string_view version,
                     std::optional<std::vector<web_app::UpdateChannel>>
                         channels = std::nullopt) {
    iwa_server_mixin_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion(version))
            .BuildBundle(GetTestKeyPair()),
        std::move(channels));
  }

  void RunUnableToInstallChecks() {
    RunUntilBrowserProcessQuits();
    EXPECT_EQ(KioskAppLaunchError::Error::kUnableToInstall,
              KioskAppLaunchError::Get());
    EXPECT_FALSE(KioskController::Get().IsSessionStarting());
  }

 private:
  GURL GetUpdateManifestUrl() const {
    return iwa_server_mixin_.GetUpdateManifestUrl(GetTestWebBundleId());
  }

  base::test::ScopedFeatureList feature_list_;
  web_app::IsolatedWebAppUpdateServerMixin iwa_server_mixin_;
  KioskMixin kiosk_mixin_;
};

struct KioskIwaUpdateChannelTestParams {
  std::string input_channel_name;
  std::optional<base::Version> expected_version;
};

// Tests how the first Kiosk IWA installation processes update channels.
class KioskIwaUpdateChannelTest
    : public KioskIwaVersionManagementBaseTest,
      public testing::WithParamInterface<KioskIwaUpdateChannelTestParams> {
 public:
  KioskIwaUpdateChannelTest()
      : KioskIwaVersionManagementBaseTest(
            KioskIwaWithCustomChannel(GetChannelName())) {
    AddTestBundle(kTestIwaVersion1);
    AddTestBundle(kTestIwaVersion2, {{kChannelBeta, kChannelAlpha}});
    AddTestBundle(kTestIwaVersion3, {{kChannelAlpha}});
  }

 protected:
  static ConfigCreator KioskIwaWithCustomChannel(
      const std::string& channel_name) {
    return base::BindOnce(&CreateManualLaunchConfigWithChannel, channel_name);
  }

  static const std::string& GetChannelName() {
    return GetParam().input_channel_name;
  }

  static const std::optional<base::Version>& GetExpectedVersion() {
    return GetParam().expected_version;
  }

  static void RunInstalledChecks() {
    ASSERT_TRUE(WaitKioskLaunched());
    EXPECT_EQ(GetIsolatedWebApp(GetTestWebAppId()).isolation_data()->version(),
              GetExpectedVersion());
  }
};

using KioskIwaUpdateChannelTestInstallSuccess = KioskIwaUpdateChannelTest;
IN_PROC_BROWSER_TEST_P(KioskIwaUpdateChannelTestInstallSuccess,
                       InstallsCorrectVersion) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  RunInstalledChecks();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaUpdateChannelTestInstallSuccess,
    testing::Values(
        // Uses 'default' channel with unset policy.
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kUnsetPolicyValue,
            .expected_version = base::Version(kTestIwaVersion1)},
        // Explicitly set 'default' channel.
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kChannelNameDefault,
            .expected_version = base::Version(kTestIwaVersion1)},
        // Installs a different version for 'beta'
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kChannelNameBeta,
            .expected_version = base::Version(kTestIwaVersion2)},
        // Selects the latest version from multiple in 'alpha'.
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kChannelNameAlpha,
            .expected_version = base::Version(kTestIwaVersion3)}));

using KioskIwaUpdateChannelTestInstallFail = KioskIwaUpdateChannelTest;
IN_PROC_BROWSER_TEST_P(KioskIwaUpdateChannelTestInstallFail, CannotInstall) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  RunUnableToInstallChecks();
}

INSTANTIATE_TEST_SUITE_P(All,
                         KioskIwaUpdateChannelTestInstallFail,
                         testing::Values(
                             // Cannot install when channel not found.
                             KioskIwaUpdateChannelTestParams{
                                 .input_channel_name = kChannelNameUnknown,
                                 .expected_version = std::nullopt}));

struct KioskIwaVersionPinningTestParams {
  std::string input_pinned_version;
  bool input_allow_downgrades;
  std::optional<base::Version> expected_version;
};

// Tests how the first Kiosk IWA installation processes version pinning.
class KioskIwaVersionPinningTest
    : public KioskIwaVersionManagementBaseTest,
      public testing::WithParamInterface<KioskIwaVersionPinningTestParams> {
 public:
  KioskIwaVersionPinningTest()
      : KioskIwaVersionManagementBaseTest(
            KioskIwaWithPinning(GetPinnedVersion(), GetAllowedDowngrades())) {
    AddTestBundle(kTestIwaVersion1);
    AddTestBundle(kTestIwaVersion2);
  }

 protected:
  static ConfigCreator KioskIwaWithPinning(const std::string& pinned_version,
                                           bool allow_downgrades) {
    return base::BindOnce(&CreateManualLaunchConfigWithVersionPinning,
                          pinned_version, allow_downgrades);
  }

  static const std::string& GetPinnedVersion() {
    return GetParam().input_pinned_version;
  }

  static bool GetAllowedDowngrades() {
    return GetParam().input_allow_downgrades;
  }

  static const std::optional<base::Version>& GetExpectedVersion() {
    return GetParam().expected_version;
  }

  static void RunInstalledVersionCheck() {
    ASSERT_TRUE(WaitKioskLaunched());
    EXPECT_EQ(GetIsolatedWebApp(GetTestWebAppId()).isolation_data()->version(),
              GetExpectedVersion());
  }
};

using KioskIwaVersionPinningTestInstallSuccess = KioskIwaVersionPinningTest;
IN_PROC_BROWSER_TEST_P(KioskIwaVersionPinningTestInstallSuccess,
                       InstallsCorrectVersion) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  RunInstalledVersionCheck();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaVersionPinningTestInstallSuccess,
    testing::Values(
        // Installs the latest version when pinning is not set.
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kUnsetPolicyValue,
            .input_allow_downgrades = false,
            .expected_version = base::Version(kTestIwaVersion2)},
        // Installs the exact pinned version, downgrading has no effect.
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersion2,
            .input_allow_downgrades = false,
            .expected_version = base::Version(kTestIwaVersion2)},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersion2,
            .input_allow_downgrades = true,
            .expected_version = base::Version(kTestIwaVersion2)},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersion1,
            .input_allow_downgrades = false,
            .expected_version = base::Version(kTestIwaVersion1)},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersion1,
            .input_allow_downgrades = true,
            .expected_version = base::Version(kTestIwaVersion1)}));

using KioskIwaVersionPinningTestInstallFail = KioskIwaVersionPinningTest;
IN_PROC_BROWSER_TEST_P(KioskIwaVersionPinningTestInstallFail,
                       CannotInstallUnknownVersion) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  RunUnableToInstallChecks();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaVersionPinningTestInstallFail,
    testing::Values(
        // Cannot install when pinned version is not found in the manifest.
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersion3,
            .input_allow_downgrades = false,
            .expected_version = std::nullopt},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersion3,
            .input_allow_downgrades = true,
            .expected_version = std::nullopt}));

using KioskIwaVersionPinningTestInvalidPolicy = KioskIwaVersionPinningTest;
IN_PROC_BROWSER_TEST_P(KioskIwaVersionPinningTestInvalidPolicy,
                       CannotCreateKioskAccount) {
  EXPECT_TRUE(KioskController::Get().GetApps().empty());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaVersionPinningTestInvalidPolicy,
    testing::Values(
        // allow_downgrades is set without a pinned version.
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kUnsetPolicyValue,
            .input_allow_downgrades = true,
            .expected_version = std::nullopt},
        // Version names that cannot be parsed.
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersionInvalid1,
            .input_allow_downgrades = false,
            .expected_version = std::nullopt},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kTestIwaVersionInvalid2,
            .input_allow_downgrades = false,
            .expected_version = std::nullopt}));

}  // namespace ash
