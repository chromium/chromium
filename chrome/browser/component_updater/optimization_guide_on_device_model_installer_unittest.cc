// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {
namespace {

using AssetManager =
    optimization_guide::ManifestAssetManagerDelegate::AssetManager;
using InstalledAsset =
    optimization_guide::ManifestAssetManagerDelegate::InstalledAsset;
using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

constexpr char kValidPublicKeyHex[] =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

class MockAssetManager : public AssetManager {
 public:
  MockAssetManager() = default;
  ~MockAssetManager() override = default;

  MOCK_METHOD(void,
              OnAssetReady,
              (const std::string& public_key,
               const base::Version& version,
               const base::FilePath& install_dir),
              (override));
  MOCK_METHOD(void,
              OnAssetUninstalled,
              (const std::string& public_key),
              (override));
  MOCK_METHOD(void,
              InstallerRegistered,
              (const std::string& public_key,
               const std::string& version,
               bool is_already_installed),
              (override));
  MOCK_METHOD(void,
              OnUpdateRequestComplete,
              (const std::string& public_key),
              (override));
  MOCK_METHOD(void, OnMeteredConnectionChanged, (), (override));

  base::WeakPtr<AssetManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockAssetManager> weak_ptr_factory_{this};
};

class MockOnDemandUpdater : public OnDemandUpdater {
 public:
  MockOnDemandUpdater() = default;
  ~MockOnDemandUpdater() override = default;

  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string& id, Priority priority, Callback callback),
              (override));
};

class OptimizationGuideOnDeviceModelInstallerTest : public testing::Test {
 protected:
  void TearDown() override {
#if !BUILDFLAG(IS_ANDROID)
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(nullptr);
#endif
  }

  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(OptimizationGuideOnDeviceModelInstallerTest, GetInstalledAssets) {
  base::ScopedPathOverride path_override(DIR_COMPONENT_USER);
  base::FilePath install_dir;
  ASSERT_TRUE(base::PathService::Get(DIR_COMPONENT_USER, &install_dir));

  const std::string kManifestModelKey =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  const std::string kAmbiguousManifestModelKey =
      "1111111111111111111111111111111111111111111111111111111111111111";
  const std::string kEmptyManifestModelKey =
      "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
  const std::string kLegacyBaseModelKey =
      "5ab6799b9cd59e4f9cbe1f4a80f5529074ea873af991002643860336a6388663";

  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideOnDeviceModel"))
          .AppendASCII("2025.8.8.1141")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kManifestModelKey)
          .AppendASCII("1.0.0.0")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kAmbiguousManifestModelKey)
          .AppendASCII("1.0.0.0")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kAmbiguousManifestModelKey)
          .AppendASCII("1.2.0.0")));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII(kEmptyManifestModelKey)));
  ASSERT_TRUE(base::CreateDirectory(
      install_dir.Append(FILE_PATH_LITERAL("OptGuideManifestModel"))
          .AppendASCII("invalid_public_key")
          .AppendASCII("1.0.0.0")));

  auto delegate = CreateManifestAssetManagerDelegate();
  base::test::TestFuture<std::vector<InstalledAsset>> future;
  delegate->GetInstalledAssets(future.GetCallback());
  EXPECT_THAT(
      future.Get(),
      testing::UnorderedElementsAre(
          InstalledAsset{kLegacyBaseModelKey, base::Version("2025.8.8.1141")},
          InstalledAsset{kManifestModelKey, base::Version("1.0.0.0")},
          InstalledAsset{kAmbiguousManifestModelKey, std::nullopt},
          InstalledAsset{kEmptyManifestModelKey, std::nullopt}));
}

TEST_F(OptimizationGuideOnDeviceModelInstallerTest,
       RequestUpdateFailsWithInvalidKeyOrMissingComponentUpdater) {
  testing::StrictMock<MockAssetManager> asset_manager;
  auto delegate = CreateManifestAssetManagerDelegate();
  delegate->SetAssetManager(asset_manager.GetWeakPtr());

  // 1. An invalid hex key completes asynchronously via `OnUpdateComplete()`.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(asset_manager, OnUpdateRequestComplete("invalid_public_key"))
        .WillOnce([&](const std::string&) { run_loop.Quit(); });
    delegate->RequestUpdate("invalid_public_key", /*is_background=*/false);
    run_loop.Run();
  }

  // 2. A valid key when `g_browser_process->component_updater()` is null also
  // completes asynchronously via `OnUpdateComplete()`.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(asset_manager, OnUpdateRequestComplete(kValidPublicKeyHex))
        .WillOnce([&](const std::string&) { run_loop.Quit(); });
    delegate->RequestUpdate(kValidPublicKeyHex, /*is_background=*/false);
    run_loop.Run();
  }
}

TEST_F(OptimizationGuideOnDeviceModelInstallerTest,
       ConnectionMeteredChangedDeduplicatesNotifications) {
  net::test::ScopedMockNetworkChangeNotifier mock_notifier;
  mock_notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  mock_notifier.mock_network_change_notifier()->SetConnectionCost(
      net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);

  testing::StrictMock<MockAssetManager> asset_manager;
  auto delegate = CreateManifestAssetManagerDelegate();
  delegate->SetAssetManager(asset_manager.GetWeakPtr());
  EXPECT_FALSE(delegate->IsConnectionMetered());

  // Redundant notifications with the same metered state do not notify
  // `asset_manager`.
  EXPECT_CALL(asset_manager, OnMeteredConnectionChanged()).Times(0);
  net::NetworkChangeNotifier::NotifyObserversOfConnectionCostChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  task_environment_.RunUntilIdle();  // nocheck

  // Transitioning to a metered connection notifies once.
  mock_notifier.mock_network_change_notifier()->SetConnectionCost(
      net::NetworkChangeNotifier::CONNECTION_COST_METERED);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(asset_manager, OnMeteredConnectionChanged()).WillOnce([&]() {
      run_loop.Quit();
    });
    net::NetworkChangeNotifier::NotifyObserversOfConnectionCostChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_COST_METERED);
    run_loop.Run();
  }
  // A follow-up `OnNetworkChanged` while still metered is deduplicated.
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_4G);
  task_environment_.RunUntilIdle();  // nocheck
  EXPECT_TRUE(delegate->IsConnectionMetered());

  // Going briefly offline (e.g. during a hotspot -> Wi-Fi handoff) reports
  // `CONNECTION_COST_UNMETERED`, but is ignored while `IsOffline()` is true so
  // the last online metered state and `IsConnectionMetered()` are preserved
  // while offline.
  mock_notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  mock_notifier.mock_network_change_notifier()->SetConnectionCost(
      net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  EXPECT_TRUE(delegate->IsConnectionMetered());
  EXPECT_CALL(asset_manager, OnMeteredConnectionChanged()).Times(0);
  net::NetworkChangeNotifier::NotifyObserversOfConnectionCostChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  task_environment_.RunUntilIdle();  // nocheck
  EXPECT_TRUE(delegate->IsConnectionMetered());

  // Once the unmetered Wi-Fi connection comes online, `OnNetworkChanged`
  // notifies `asset_manager` once.
  mock_notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(asset_manager, OnMeteredConnectionChanged()).WillOnce([&]() {
      run_loop.Quit();
    });
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
    run_loop.Run();
  }
  EXPECT_FALSE(delegate->IsConnectionMetered());

  // Going offline from an unmetered connection and reconnecting to the same
  // unmetered Wi-Fi connection also notifies `asset_manager` on reconnect so
  // downloads that failed while offline can be retried.
  mock_notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  EXPECT_CALL(asset_manager, OnMeteredConnectionChanged()).Times(0);
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  task_environment_.RunUntilIdle();  // nocheck
  EXPECT_FALSE(delegate->IsConnectionMetered());

  mock_notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(asset_manager, OnMeteredConnectionChanged()).WillOnce([&]() {
      run_loop.Quit();
    });
    net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
    run_loop.Run();
  }
  EXPECT_FALSE(delegate->IsConnectionMetered());
}

// `TestingBrowserProcess::SetComponentUpdater` is only available on non-Android
// platforms.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(OptimizationGuideOnDeviceModelInstallerTest,
       RequestUpdateCompletionAndInvalidation) {
  auto cus = std::make_unique<testing::NiceMock<MockComponentUpdateService>>();
  MockComponentUpdateService* cus_ptr = cus.get();
  MockOnDemandUpdater on_demand_updater;
  ON_CALL(*cus_ptr, GetOnDemandUpdater())
      .WillByDefault(ReturnRef(on_demand_updater));
  TestingBrowserProcess::GetGlobal()->SetComponentUpdater(std::move(cus));

  testing::StrictMock<MockAssetManager> asset_manager;
  auto delegate = CreateManifestAssetManagerDelegate();
  delegate->SetAssetManager(asset_manager.GetWeakPtr());

  const std::string crx_id =
      *optimization_guide::GetCrxIdFromPublicKeyHex(kValidPublicKeyHex);

  // 1. Background `UPDATE_DISABLED` and foreground `NONE` both notify
  // `OnUpdateRequestComplete`.
  for (auto [is_background, priority, error] : {
           std::tuple{true, OnDemandUpdater::Priority::BACKGROUND,
                      update_client::Error::UPDATE_DISABLED},
           std::tuple{false, OnDemandUpdater::Priority::FOREGROUND,
                      update_client::Error::NONE},
       }) {
    base::RunLoop run_loop;
    EXPECT_CALL(on_demand_updater, OnDemandUpdate(crx_id, priority, _))
        .WillOnce([error](const std::string&, OnDemandUpdater::Priority,
                          Callback callback) {
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(std::move(callback), error));
        });
    EXPECT_CALL(asset_manager, OnUpdateRequestComplete(kValidPublicKeyHex))
        .WillOnce([&](const std::string&) { run_loop.Quit(); });
    delegate->RequestUpdate(kValidPublicKeyHex, is_background);
    run_loop.Run();
  }

  // 2. A second `RequestUpdate()` supersedes the first, and `Uninstall()`
  // invalidates an in-flight request.
  Callback first_callback;
  Callback second_callback;
  Callback third_callback;
  EXPECT_CALL(on_demand_updater,
              OnDemandUpdate(crx_id, OnDemandUpdater::Priority::FOREGROUND, _))
      .WillOnce(
          [&](const std::string&, OnDemandUpdater::Priority,
              Callback callback) { first_callback = std::move(callback); })
      .WillOnce(
          [&](const std::string&, OnDemandUpdater::Priority,
              Callback callback) { second_callback = std::move(callback); })
      .WillOnce(
          [&](const std::string&, OnDemandUpdater::Priority,
              Callback callback) { third_callback = std::move(callback); });

  delegate->RequestUpdate(kValidPublicKeyHex, /*is_background=*/false);
  delegate->RequestUpdate(kValidPublicKeyHex, /*is_background=*/false);

  EXPECT_CALL(asset_manager, OnUpdateRequestComplete(_)).Times(0);
  std::move(first_callback).Run(update_client::Error::UPDATE_CANCELED);

  EXPECT_CALL(asset_manager, OnUpdateRequestComplete(kValidPublicKeyHex))
      .Times(1);
  std::move(second_callback).Run(update_client::Error::UPDATE_CANCELED);

  delegate->RequestUpdate(kValidPublicKeyHex, /*is_background=*/false);
  EXPECT_CALL(*cus_ptr, UnregisterComponent(crx_id)).WillOnce(Return(true));
  delegate->Uninstall(kValidPublicKeyHex);

  EXPECT_CALL(asset_manager, OnUpdateRequestComplete(_)).Times(0);
  std::move(third_callback).Run(update_client::Error::NONE);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace component_updater
