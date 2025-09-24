// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_data.h"
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_apply_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/profile_waiter.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/test_support/signed_web_bundles/key_pair.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/test_support/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

using base::test::ErrorIs;
using base::test::HasValue;
using base::test::ValueIs;
using kiosk::test::BlockKioskLaunch;
using kiosk::test::CreateDeviceLocalAccountId;
using kiosk::test::CurrentProfile;
using kiosk::test::LaunchAppManually;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

constexpr char kTestAccountId[] = "kiosk-iwa-test@localhost";

constexpr char kTestIwaTitle1[] = "First app title";
constexpr char kTestIwaTitle2[] = "Changed title";

constexpr char kVersionString1[] = "1";
constexpr char kVersionString2[] = "2.0";
constexpr char kVersionString3[] = "3.0.0";
constexpr char kVersionStringUnknown[] = "9.9.9";

constexpr char kVersionStringInvalid1[] = "not a version number";
constexpr char kVersionStringInvalid2[] = "2,0";

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

AccountId GetTestDeviceLocalAccountId() {
  return CreateDeviceLocalAccountId(
      GetTestAccountId(), policy::DeviceLocalAccountType::kKioskIsolatedWebApp);
}

const KioskIwaData* GetCurrentKioskIwaData() {
  return CHECK_DEREF(KioskIwaManager::Get())
      .GetApp(GetTestDeviceLocalAccountId());
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
  return CHECK_DEREF(web_app::WebAppProvider::GetForTest(&CurrentProfile()));
}

void WaitForKioskProfile() {
  ProfileWaiter waiter;
  Profile* profile = waiter.WaitForProfileAdded();

  // Checks that it's the same profile used in kiosk test utils.
  ASSERT_TRUE(CurrentProfile().IsSameOrParent(profile));
}

web_app::IsolatedWebAppUpdateDiscoveryTask::CompletionStatus
WaitForTestAppUpdateDiscovery() {
  using UpdateDiscoveryTaskFuture = base::test::TestFuture<
      web_app::IsolatedWebAppUpdateDiscoveryTask::CompletionStatus>;

  UpdateDiscoveryTaskFuture update_discovery_future;
  web_app::UpdateDiscoveryTaskResultWaiter update_discovery_waiter(
      GetWebAppProvider(), GetTestWebAppId(),
      update_discovery_future.GetCallback());
  return update_discovery_future.Take();
}

web_app::IsolatedWebAppApplyUpdateCommandResult WaitForTestAppUpdateApply() {
  using UpdateApplyTaskFuture =
      base::test::TestFuture<web_app::IsolatedWebAppApplyUpdateCommandResult>;

  UpdateApplyTaskFuture update_apply_future;
  web_app::UpdateApplyTaskResultWaiter update_apply_waiter(
      GetWebAppProvider(), GetTestWebAppId(),
      update_apply_future.GetCallback());
  return update_apply_future.Take();
}

const web_app::WebApp& GetIsolatedWebApp(const webapps::AppId& app_id) {
  return CHECK_DEREF(GetWebAppProvider().registrar_unsafe().GetAppById(app_id));
}

void ExpectTestAppInstalledAtVersion(
    const web_app::IwaVersion& expected_version) {
  EXPECT_EQ(GetIsolatedWebApp(GetTestWebAppId()).isolation_data()->version(),
            expected_version);
}

void ExpectTestAppUpdatedToVersion(
    const web_app::IwaVersion& expected_version) {
  const auto update_apply_status = WaitForTestAppUpdateApply();
  EXPECT_THAT(update_apply_status, HasValue());
  ExpectTestAppInstalledAtVersion(expected_version);
}

void ExpectAppUpdateSkipped() {
  ASSERT_THAT(
      WaitForTestAppUpdateDiscovery(),
      ValueIs(
          web_app::IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound));
}

void ExpectAppUpdateDiscovered() {
  ASSERT_THAT(WaitForTestAppUpdateDiscovery(),
              ValueIs(web_app::IsolatedWebAppUpdateDiscoveryTask::Success::
                          kUpdateFoundAndSavedInDatabase));
}

void ExpectAppDowngradeDiscovered() {
  ASSERT_THAT(WaitForTestAppUpdateDiscovery(),
              ValueIs(web_app::IsolatedWebAppUpdateDiscoveryTask::Success::
                          kDowngradeVersionFoundAndSavedInDatabase));
}

void ExpectAppPinnedVersionDiscovered() {
  ASSERT_THAT(WaitForTestAppUpdateDiscovery(),
              ValueIs(web_app::IsolatedWebAppUpdateDiscoveryTask::Success::
                          kPinnedVersionUpdateFoundAndSavedInDatabase));
}

void ExpectDowngradeNotAllowed() {
  ASSERT_THAT(WaitForTestAppUpdateDiscovery(),
              ErrorIs(web_app::IsolatedWebAppUpdateDiscoveryTask::Error::
                          kDowngradetNotAllowed));
}

void ExpectNoApplicableVersion() {
  EXPECT_THAT(WaitForTestAppUpdateDiscovery(),
              ErrorIs(web_app::IsolatedWebAppUpdateDiscoveryTask::Error::
                          kUpdateManifestNoApplicableVersion));
}

void ExpectPinnedVersionNotFoundInUpdateManifest() {
  EXPECT_THAT(WaitForTestAppUpdateDiscovery(),
              ErrorIs(web_app::IsolatedWebAppUpdateDiscoveryTask::Error::
                          kPinnedVersionNotFoundInUpdateManifest));
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

// Checks that Kiosk IWA name that is used in the Apps menu is updated.
// Wraps KioskAppManagerObserver to ignore irrelevant web app updates
// (e.g. empty 'update' event from initialization or an icon key update).
class KioskIwaTitleChangeWaiter : public KioskAppManagerObserver {
 public:
  explicit KioskIwaTitleChangeWaiter(std::string expected_title)
      : expected_title_(std::move(expected_title)) {
    iwa_manager_observation_.Observe(KioskIwaManager::Get());
  }
  KioskIwaTitleChangeWaiter(const KioskIwaTitleChangeWaiter&) = delete;
  KioskIwaTitleChangeWaiter& operator=(const KioskIwaTitleChangeWaiter&) =
      delete;
  ~KioskIwaTitleChangeWaiter() override = default;

  // `KioskAppManagerObserver`:
  void OnKioskAppDataChanged(const std::string& id_of_updated_app) override {
    if (id_of_updated_app == GetTestWebAppId() &&
        TheKioskApp().name() == expected_title_) {
      title_change_future_.SetValue(true);
    }
  }

  bool Wait() { return title_change_future_.Take(); }

 private:
  const std::string expected_title_;
  base::test::TestFuture<bool> title_change_future_;

  base::ScopedObservation<KioskIwaManager, KioskAppManagerObserver>
      iwa_manager_observation_{this};
};

}  // namespace

// Base class for Kiosk IWA version management fixtures.
class KioskIwaVersionManagementBaseTest
    : public MixinBasedInProcessBrowserTest {
 public:
  // Factory method to create kiosk mixin configurations based on the url from
  // IsolatedWebAppTestUpdateServer.
  using ConfigCreator =
      base::OnceCallback<KioskMixin::Config(const GURL& update_manifest_url)>;

  explicit KioskIwaVersionManagementBaseTest(ConfigCreator config_creator)
      : kiosk_mixin_(&mixin_host_,
                     std::move(config_creator).Run(GetUpdateManifestUrl())) {
    // Prevents a race condition (often in debug builds) where a network service
    // process is not yet started when kiosk downloads the IWA update manifest.
    content::ForceInProcessNetworkService();
  }

  ~KioskIwaVersionManagementBaseTest() override = default;
  KioskIwaVersionManagementBaseTest(const KioskIwaVersionManagementBaseTest&) =
      delete;
  KioskIwaVersionManagementBaseTest& operator=(
      const KioskIwaVersionManagementBaseTest&) = delete;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    OverrideCacheDir();
  }

  void AddTestBundle(std::string_view version,
                     std::optional<std::vector<web_app::UpdateChannel>>
                         channels = std::nullopt) {
    iwa_test_update_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetVersion(version))
            .BuildBundle(GetTestKeyPair()),
        std::move(channels));
  }

  void AddTestBundle(std::string_view name,
                     std::string_view version,
                     std::optional<std::vector<web_app::UpdateChannel>>
                         channels = std::nullopt) {
    iwa_test_update_server_.AddBundle(
        web_app::IsolatedWebAppBuilder(
            web_app::ManifestBuilder().SetName(name).SetVersion(version))
            .BuildBundle(GetTestKeyPair()),
        std::move(channels));
  }

  void RunUnableToInstallChecks() {
    RunUntilBrowserProcessQuits();
    EXPECT_EQ(KioskAppLaunchError::Error::kUnableToInstall,
              KioskAppLaunchError::Get(
                  CHECK_DEREF(g_browser_process->local_state())));
    EXPECT_FALSE(KioskController::Get().IsSessionStarting());
  }

 private:
  GURL GetUpdateManifestUrl() const {
    return iwa_test_update_server_.GetUpdateManifestUrl(GetTestWebBundleId());
  }

  void OverrideCacheDir() {
    // `IsolatedWebAppUpdateManager` saves the updated version to cache, if the
    // cache directory is not set, it will result with a failure.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    ASSERT_TRUE(profile_manager);
    cache_root_dir_ = profile_manager->user_data_dir();
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_);
  }

  web_app::IsolatedWebAppTestUpdateServer iwa_test_update_server_;
  KioskMixin kiosk_mixin_;
  base::FilePath cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
};

struct KioskIwaUpdateChannelTestParams {
  std::string input_channel_name;
  std::optional<web_app::IwaVersion> expected_version;
};

// Tests how the first Kiosk IWA installation processes update channels.
class KioskIwaUpdateChannelTest
    : public KioskIwaVersionManagementBaseTest,
      public testing::WithParamInterface<KioskIwaUpdateChannelTestParams> {
 public:
  KioskIwaUpdateChannelTest()
      : KioskIwaVersionManagementBaseTest(
            KioskIwaWithCustomChannel(GetChannelName())) {
    AddTestBundle(kVersionString1);
    AddTestBundle(kVersionString2, {{kChannelBeta, kChannelAlpha}});
    AddTestBundle(kVersionString3, {{kChannelAlpha}});
  }

 protected:
  static ConfigCreator KioskIwaWithCustomChannel(
      const std::string& channel_name) {
    return base::BindOnce(&CreateManualLaunchConfigWithChannel, channel_name);
  }

  static const std::string& GetChannelName() {
    return GetParam().input_channel_name;
  }

  static const web_app::IwaVersion& GetExpectedVersion() {
    return GetParam().expected_version.value();
  }
};

using KioskIwaUpdateChannelTestInstallSuccess = KioskIwaUpdateChannelTest;
IN_PROC_BROWSER_TEST_P(KioskIwaUpdateChannelTestInstallSuccess,
                       InstallsCorrectVersion) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ExpectTestAppInstalledAtVersion(GetExpectedVersion());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaUpdateChannelTestInstallSuccess,
    testing::Values(
        // Uses 'default' channel with unset policy.
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kUnsetPolicyValue,
            .expected_version = *web_app::IwaVersion::Create(kVersionString1)},
        // Explicitly set 'default' channel.
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kChannelNameDefault,
            .expected_version = *web_app::IwaVersion::Create(kVersionString1)},
        // Installs a different version for 'beta'.
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kChannelNameBeta,
            .expected_version = *web_app::IwaVersion::Create(kVersionString2)},
        // Selects the latest version from multiple in 'alpha'.
        KioskIwaUpdateChannelTestParams{
            .input_channel_name = kChannelNameAlpha,
            .expected_version =
                *web_app::IwaVersion::Create(kVersionString3)}));

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
  std::optional<web_app::IwaVersion> expected_version;
};

// Tests how the first Kiosk IWA installation processes version pinning.
class KioskIwaVersionPinningTest
    : public KioskIwaVersionManagementBaseTest,
      public testing::WithParamInterface<KioskIwaVersionPinningTestParams> {
 public:
  KioskIwaVersionPinningTest()
      : KioskIwaVersionManagementBaseTest(
            KioskIwaWithPinning(GetPinnedVersion(), GetAllowedDowngrades())) {
    AddTestBundle(kVersionString1);
    AddTestBundle(kVersionString2);
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

  static const web_app::IwaVersion& GetExpectedVersion() {
    return GetParam().expected_version.value();
  }
};

using KioskIwaVersionPinningTestInstallSuccess = KioskIwaVersionPinningTest;
IN_PROC_BROWSER_TEST_P(KioskIwaVersionPinningTestInstallSuccess,
                       InstallsCorrectVersion) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ExpectTestAppInstalledAtVersion(GetExpectedVersion());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaVersionPinningTestInstallSuccess,
    testing::Values(
        // Installs the latest version when pinning is not set.
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kUnsetPolicyValue,
            .input_allow_downgrades = false,
            .expected_version = *web_app::IwaVersion::Create(kVersionString2)},
        // Installs the exact pinned version, downgrading has no effect.
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kVersionString2,
            .input_allow_downgrades = false,
            .expected_version = *web_app::IwaVersion::Create(kVersionString2)},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kVersionString2,
            .input_allow_downgrades = true,
            .expected_version = *web_app::IwaVersion::Create(kVersionString2)},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kVersionString1,
            .input_allow_downgrades = false,
            .expected_version = *web_app::IwaVersion::Create(kVersionString1)},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kVersionString1,
            .input_allow_downgrades = true,
            .expected_version =
                *web_app::IwaVersion::Create(kVersionString1)}));

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
            .input_pinned_version = kVersionString3,
            .input_allow_downgrades = false,
            .expected_version = std::nullopt},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kVersionString3,
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
            .input_pinned_version = kVersionStringInvalid1,
            .input_allow_downgrades = false,
            .expected_version = std::nullopt},
        KioskIwaVersionPinningTestParams{
            .input_pinned_version = kVersionStringInvalid2,
            .input_allow_downgrades = false,
            .expected_version = std::nullopt}));

// Tests an app update to the new latest version in the same channel.
class KioskIwaSimpleUpdateTest : public KioskIwaVersionManagementBaseTest {
 public:
  KioskIwaSimpleUpdateTest()
      : KioskIwaVersionManagementBaseTest(KioskIwaWithDefaultChannel()) {
    AddTestBundle(kTestIwaTitle1, kVersionString1);
  }

 protected:
  static ConfigCreator KioskIwaWithDefaultChannel() {
    return base::BindOnce(&CreateManualLaunchConfigWithChannel,
                          kChannelNameDefault);
  }
};

IN_PROC_BROWSER_TEST_F(KioskIwaSimpleUpdateTest,
                       PRE_UpdatesToLatestBeforeLaunch) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ExpectTestAppInstalledAtVersion(
      *web_app::IwaVersion::Create(kVersionString1));
}

IN_PROC_BROWSER_TEST_F(KioskIwaSimpleUpdateTest, UpdatesToLatestBeforeLaunch) {
  EXPECT_EQ(TheKioskApp().name(), kTestIwaTitle1);

  AddTestBundle(kTestIwaTitle2, kVersionString2);
  const web_app::IwaVersion expected_version =
      *web_app::IwaVersion::Create(kVersionString2);

  // Prevents the app launch to let the update apply.
  auto scoped_launch_blocker = BlockKioskLaunch();
  KioskIwaTitleChangeWaiter title_change_waiter(kTestIwaTitle2);
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitForKioskProfile();
  ExpectTestAppUpdatedToVersion(expected_version);

  ExpectTestAppInstalledAtVersion(expected_version);
  ASSERT_TRUE(title_change_waiter.Wait());
}

IN_PROC_BROWSER_TEST_F(KioskIwaSimpleUpdateTest, PRE_UpdatesToLatestAtExit) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ExpectTestAppInstalledAtVersion(
      *web_app::IwaVersion::Create(kVersionString1));
}

IN_PROC_BROWSER_TEST_F(KioskIwaSimpleUpdateTest, UpdatesToLatestAtExit) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  // Wait for the first update discovery to finish.
  ExpectAppUpdateSkipped();

  AddTestBundle(kVersionString2);
  EXPECT_EQ(GetWebAppProvider().iwa_update_manager().DiscoverUpdatesNow(), 1UL);

  ExpectAppUpdateDiscovered();
  kiosk::test::CloseAppWindow(TheKioskApp());
  ExpectTestAppUpdatedToVersion(*web_app::IwaVersion::Create(kVersionString2));
}

struct KioskIwaUpdateChannelChangeTestParams {
  enum class TestCase {
    kUpdateApplied,
    kUpdateSkipped,
    kUpdateError,
    kDowngradeNotAllowed
  };

  TestCase test_case;
  std::string initial_channel_name;
  web_app::IwaVersion expected_initial_version;
  std::string new_channel_name;
  web_app::IwaVersion expected_new_version;
};

// Tests how the Kiosk IWA update processes update channels.
class KioskIwaUpdateChannelChangeTest
    : public KioskIwaVersionManagementBaseTest,
      public testing::WithParamInterface<
          KioskIwaUpdateChannelChangeTestParams> {
 public:
  KioskIwaUpdateChannelChangeTest()
      : KioskIwaVersionManagementBaseTest(KioskIwaWithChannelSwitch()) {
    AddTestBundle(kVersionString1);
    AddTestBundle(kVersionString2, {{kChannelBeta}});
    AddTestBundle(kVersionString3, {{kChannelAlpha}});
  }

 protected:
  static ConfigCreator KioskIwaWithChannelSwitch() {
    const std::string& channel_name =
        content::IsPreTest() ? GetInitialChannelName() : GetNewChannelName();
    return base::BindOnce(&CreateManualLaunchConfigWithChannel, channel_name);
  }

  static KioskIwaUpdateChannelChangeTestParams::TestCase GetTestCase() {
    return GetParam().test_case;
  }

  static const std::string& GetInitialChannelName() {
    return GetParam().initial_channel_name;
  }

  static const web_app::IwaVersion& GetExpectedInitialVersion() {
    return GetParam().expected_initial_version;
  }

  static const std::string& GetNewChannelName() {
    return GetParam().new_channel_name;
  }

  static const web_app::IwaVersion& GetExpectedNewVersion() {
    return GetParam().expected_new_version;
  }

  static void CheckUpdateStatusForTestCase() {
    switch (GetTestCase()) {
      case KioskIwaUpdateChannelChangeTestParams::TestCase::kUpdateApplied:
        ExpectTestAppUpdatedToVersion(GetExpectedNewVersion());
        break;
      case KioskIwaUpdateChannelChangeTestParams::TestCase::kUpdateSkipped:
        ExpectAppUpdateSkipped();
        break;
      case KioskIwaUpdateChannelChangeTestParams::TestCase::kUpdateError:
        ExpectNoApplicableVersion();
        break;
      case KioskIwaUpdateChannelChangeTestParams::TestCase::
          kDowngradeNotAllowed:
        ExpectDowngradeNotAllowed();
        break;
    }
  }
};

IN_PROC_BROWSER_TEST_P(KioskIwaUpdateChannelChangeTest,
                       PRE_ProcessChannelChange) {
  EXPECT_EQ(GetCurrentKioskIwaData()->update_channel().ToString(),
            GetInitialChannelName());
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ExpectTestAppInstalledAtVersion(GetExpectedInitialVersion());
}

IN_PROC_BROWSER_TEST_P(KioskIwaUpdateChannelChangeTest, ProcessChannelChange) {
  EXPECT_EQ(GetCurrentKioskIwaData()->update_channel().ToString(),
            GetNewChannelName());

  // Prevents the app launch to let the app update apply.
  auto scoped_launch_blocker = BlockKioskLaunch();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitForKioskProfile();
  CheckUpdateStatusForTestCase();

  ExpectTestAppInstalledAtVersion(GetExpectedNewVersion());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaUpdateChannelChangeTest,
    testing::Values(
        // Switching to a channel with a newer version updates the app.
        // Switch from "default" to "beta".
        KioskIwaUpdateChannelChangeTestParams{
            .test_case =
                KioskIwaUpdateChannelChangeTestParams::TestCase::kUpdateApplied,
            .initial_channel_name = kChannelNameDefault,
            .expected_initial_version =
                *web_app::IwaVersion::Create(kVersionString1),
            .new_channel_name = kChannelNameBeta,
            .expected_new_version =
                *web_app::IwaVersion::Create(kVersionString2)},
        // Switch from "default" to "alpha".
        KioskIwaUpdateChannelChangeTestParams{
            .test_case =
                KioskIwaUpdateChannelChangeTestParams::TestCase::kUpdateApplied,
            .initial_channel_name = kChannelNameDefault,
            .expected_initial_version =
                *web_app::IwaVersion::Create(kVersionString1),
            .new_channel_name = kChannelNameAlpha,
            .expected_new_version =
                *web_app::IwaVersion::Create(kVersionString3)},
        // Switch from "beta" to "alpha".
        KioskIwaUpdateChannelChangeTestParams{
            .test_case =
                KioskIwaUpdateChannelChangeTestParams::TestCase::kUpdateApplied,
            .initial_channel_name = kChannelNameBeta,
            .expected_initial_version =
                *web_app::IwaVersion::Create(kVersionString2),
            .new_channel_name = kChannelNameAlpha,
            .expected_new_version =
                *web_app::IwaVersion::Create(kVersionString3)},

        // Switching to a channel with an older version skips the update.
        // Switch from "beta" to "default".
        KioskIwaUpdateChannelChangeTestParams{
            .test_case = KioskIwaUpdateChannelChangeTestParams::TestCase::
                kDowngradeNotAllowed,
            .initial_channel_name = kChannelNameBeta,
            .expected_initial_version =
                *web_app::IwaVersion::Create(kVersionString2),
            .new_channel_name = kChannelNameDefault,
            .expected_new_version =
                *web_app::IwaVersion::Create(kVersionString2)},
        // Switch from "alpha" (at version 3.0.0) to "default" (latest
        // version: 2.0, downgrade not possible).
        KioskIwaUpdateChannelChangeTestParams{
            .test_case = KioskIwaUpdateChannelChangeTestParams::TestCase::
                kDowngradeNotAllowed,
            .initial_channel_name = kChannelNameAlpha,
            .expected_initial_version =
                *web_app::IwaVersion::Create(kVersionString3),
            .new_channel_name = kChannelNameDefault,
            .expected_new_version =
                *web_app::IwaVersion::Create(kVersionString3)},

        // Switching to an unknown channel skips the update with an error.
        // Switch from "beta" to "unknown".
        KioskIwaUpdateChannelChangeTestParams{
            .test_case =
                KioskIwaUpdateChannelChangeTestParams::TestCase::kUpdateError,
            .initial_channel_name = kChannelNameBeta,
            .expected_initial_version =
                *web_app::IwaVersion::Create(kVersionString2),
            .new_channel_name = kChannelNameUnknown,
            .expected_new_version =
                *web_app::IwaVersion::Create(kVersionString2)}));

struct KioskIwaVersionPinningUpdateTestParams {
  enum class TestCase {
    kNoUpdateQueued,
    kUpdateApplied,
    kUpdateToPinnedVersionApplied,
    kDowngradeToPinnedVersionApplied,
    kUpdateNotFound,
    kPinnedVersionNotFound
  };

  TestCase test_case;
  std::string input_pinned_version;
  bool input_allow_downgrades;
  web_app::IwaVersion expected_version;
};

// Tests how the Kiosk IWA update processes version pinning.
class KioskIwaVersionPinningUpdateTest
    : public KioskIwaVersionManagementBaseTest,
      public testing::WithParamInterface<
          KioskIwaVersionPinningUpdateTestParams> {
 public:
  KioskIwaVersionPinningUpdateTest()
      : KioskIwaVersionManagementBaseTest(KioskIwaWithPinningUpdate()) {
    AddTestBundle(kVersionString1);
    AddTestBundle(kVersionString2);
    if (!content::IsPreTest()) {
      AddTestBundle(kVersionString3);
    }
  }

 protected:
  static ConfigCreator KioskIwaWithPinningUpdate() {
    if (content::IsPreTest()) {
      return base::BindOnce(&CreateManualLaunchConfigWithChannel,
                            kUnsetPolicyValue);
    }

    return base::BindOnce(&CreateManualLaunchConfigWithVersionPinning,
                          GetPinnedVersion(), GetAllowedDowngrades());
  }

  static KioskIwaVersionPinningUpdateTestParams::TestCase GetTestCase() {
    return GetParam().test_case;
  }

  static const std::string& GetPinnedVersion() {
    return GetParam().input_pinned_version;
  }

  static bool GetAllowedDowngrades() {
    return GetParam().input_allow_downgrades;
  }

  static const web_app::IwaVersion& GetExpectedVersion() {
    return GetParam().expected_version;
  }

  static void CheckUpdateStatusForTestCase() {
    switch (GetTestCase()) {
      case KioskIwaVersionPinningUpdateTestParams::TestCase::kNoUpdateQueued:
        EXPECT_EQ(GetWebAppProvider().iwa_update_manager().DiscoverUpdatesNow(),
                  0UL);
        break;
      case KioskIwaVersionPinningUpdateTestParams::TestCase::kUpdateApplied:
        ExpectAppPinnedVersionDiscovered();
        ExpectTestAppUpdatedToVersion(GetExpectedVersion());
        break;
      case KioskIwaVersionPinningUpdateTestParams::TestCase::
          kDowngradeToPinnedVersionApplied:
        ExpectAppDowngradeDiscovered();
        ExpectTestAppUpdatedToVersion(GetExpectedVersion());
        break;
      case KioskIwaVersionPinningUpdateTestParams::TestCase::
          kUpdateToPinnedVersionApplied:
        ExpectTestAppUpdatedToVersion(GetExpectedVersion());
        break;
      case KioskIwaVersionPinningUpdateTestParams::TestCase::kUpdateNotFound:
        ExpectNoApplicableVersion();
        break;
      case KioskIwaVersionPinningUpdateTestParams::TestCase::
          kPinnedVersionNotFound:
        ExpectPinnedVersionNotFoundInUpdateManifest();
        break;
    }
  }
};

IN_PROC_BROWSER_TEST_P(KioskIwaVersionPinningUpdateTest, PRE_ProcessPinning) {
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ExpectTestAppInstalledAtVersion(
      *web_app::IwaVersion::Create(kVersionString2));
}

IN_PROC_BROWSER_TEST_P(KioskIwaVersionPinningUpdateTest, ProcessPinning) {
  // Prevents the app launch to let the app update apply.
  auto scoped_launch_blocker = BlockKioskLaunch();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  WaitForKioskProfile();
  CheckUpdateStatusForTestCase();

  ExpectTestAppInstalledAtVersion(GetExpectedVersion());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskIwaVersionPinningUpdateTest,
    testing::Values(
        // Pinning to the current version doesn't update.
        KioskIwaVersionPinningUpdateTestParams{
            .test_case = KioskIwaVersionPinningUpdateTestParams::TestCase::
                kNoUpdateQueued,
            .input_pinned_version = kVersionString2,
            .input_allow_downgrades = false,
            .expected_version = *web_app::IwaVersion::Create(kVersionString2)},
        // Pinning to the newer version updates the app.
        KioskIwaVersionPinningUpdateTestParams{
            .test_case = KioskIwaVersionPinningUpdateTestParams::TestCase::
                kUpdateToPinnedVersionApplied,
            .input_pinned_version = kVersionString3,
            .input_allow_downgrades = false,
            .expected_version = *web_app::IwaVersion::Create(kVersionString3)},
        // Pinning to the older version without allow_downgrades doesn't update.
        KioskIwaVersionPinningUpdateTestParams{
            .test_case = KioskIwaVersionPinningUpdateTestParams::TestCase::
                kNoUpdateQueued,
            .input_pinned_version = kVersionString1,
            .input_allow_downgrades = false,
            .expected_version = *web_app::IwaVersion::Create(kVersionString2)},
        // Pinning to the older version with allow_downgrades updates the app.
        KioskIwaVersionPinningUpdateTestParams{
            .test_case = KioskIwaVersionPinningUpdateTestParams::TestCase::
                kDowngradeToPinnedVersionApplied,
            .input_pinned_version = kVersionString1,
            .input_allow_downgrades = true,
            .expected_version = *web_app::IwaVersion::Create(kVersionString1)},
        // Pinning to the unknown version skips the update with an error.
        KioskIwaVersionPinningUpdateTestParams{
            .test_case = KioskIwaVersionPinningUpdateTestParams::TestCase::
                kPinnedVersionNotFound,
            .input_pinned_version = kVersionStringUnknown,
            .input_allow_downgrades = true,
            .expected_version =
                *web_app::IwaVersion::Create(kVersionString2)}));

}  // namespace ash
