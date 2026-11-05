// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_future.h"
#include "chrome/browser/component_updater/ai_embeddings_component_installer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/model_execution/on_device_prefs.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace {

optimization_guide::ModelInfo GetTestModelInfo() {
  return optimization_guide::ModelInfo{
      .model_file_path = base::FilePath(FILE_PATH_LITERAL("embeddings")),
      .additional_files = {base::FilePath(FILE_PATH_LITERAL("sp"))},
      .version = 1,
      .model_metadata = optimization_guide::AnyWrapProto(
          optimization_guide::proto::PassageEmbeddingsModelMetadata()),
  };
}

class AISemanticEmbedderServiceLauncherForTest
    : public AISemanticEmbedderServiceLauncher {};

class MockOnDemandUpdater : public component_updater::OnDemandUpdater {
 public:
  MOCK_METHOD(void,
              OnDemandUpdate,
              (const std::string&, Priority, component_updater::Callback),
              (override));
};

}  // namespace

class AISemanticEmbedderServiceLauncherTest : public testing::Test {
 public:
  AISemanticEmbedderServiceLauncherTest() {
    scoped_feature_list_.InitWithFeatures({blink::features::kAIEmbeddingsAPI},
                                          {});
  }

  void SetUp() override {
    auto mock_cus = std::make_unique<
        testing::NiceMock<component_updater::MockComponentUpdateService>>();
    mock_cus_ = mock_cus.get();
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(
        std::move(mock_cus));
    ON_CALL(*mock_cus_, GetOnDemandUpdater())
        .WillByDefault(testing::ReturnRef(mock_on_demand_updater_));
    // The "On-device AI" user setting defaults to off on non-official builds.
    // Model downloads require it, so turn it on for the tests.
    TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
        optimization_guide::model_execution::prefs::localstate::
            kOnDeviceAiUserSettingsEnabled,
        true);
  }

  void TearDown() override {
    mock_cus_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(nullptr);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Uninstalling deletes files under DIR_COMPONENT_USER, which unit tests
  // otherwise resolve to the real user data directory. Declared before
  // `task_environment_` so it outlives the pending deletions that run when the
  // task environment is destroyed.
  base::ScopedPathOverride component_user_dir_override_{
      component_updater::DIR_COMPONENT_USER};
  // Uninstalling posts to the UI thread, which needs a BrowserTaskEnvironment.
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<testing::NiceMock<component_updater::MockComponentUpdateService>>
      mock_cus_ = nullptr;
  testing::NiceMock<MockOnDemandUpdater> mock_on_demand_updater_;
};

TEST_F(AISemanticEmbedderServiceLauncherTest, InitiallyAllowedToLaunch) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  EXPECT_TRUE(launcher.AllowedToLaunch());
}

TEST_F(AISemanticEmbedderServiceLauncherTest,
       IdleDisconnectDoesNotCountAsCrash) {
  AISemanticEmbedderServiceLauncherForTest launcher;

  launcher.OnServiceDisconnected(/*is_idle=*/true);
  launcher.OnServiceDisconnected(/*is_idle=*/true);
  launcher.OnServiceDisconnected(/*is_idle=*/true);
  launcher.OnServiceDisconnected(/*is_idle=*/true);

  EXPECT_TRUE(launcher.AllowedToLaunch());
}

TEST_F(AISemanticEmbedderServiceLauncherTest, CrashDisconnectThrottlesLaunch) {
  AISemanticEmbedderServiceLauncherForTest launcher;

  launcher.OnServiceDisconnected(/*is_idle=*/false);
  EXPECT_TRUE(launcher.AllowedToLaunch());

  launcher.OnServiceDisconnected(/*is_idle=*/false);
  EXPECT_TRUE(launcher.AllowedToLaunch());

  launcher.OnServiceDisconnected(/*is_idle=*/false);
  EXPECT_FALSE(launcher.AllowedToLaunch());
}

TEST_F(AISemanticEmbedderServiceLauncherTest,
       WaitForModelAvailable_AlreadyReady) {
  AISemanticEmbedderServiceLauncherForTest launcher;

  // Set the model to ready immediately.
  launcher.controller()->MaybeUpdateModelInfo(GetTestModelInfo());
  EXPECT_TRUE(launcher.controller()->IsModelAvailable());

  // Callback should execute immediately (synchronously).
  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
}

TEST_F(AISemanticEmbedderServiceLauncherTest,
       WaitForModelAvailable_QueuedAndResolved) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .WillOnce([&run_loop](const std::string&,
                            component_updater::OnDemandUpdater::Priority,
                            component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
        run_loop.Quit();
      });

  // Call WaitForModelAvailable multiple times, they should all be queued.
  base::test::TestFuture<void> future1;
  base::test::TestFuture<void> future2;
  launcher.WaitForModelAvailable(future1.GetCallback());
  launcher.WaitForModelAvailable(future2.GetCallback());

  // They should not have executed yet.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Wait for the component registration to finish and trigger OnDemandUpdate.
  run_loop.Run();

  // Simulate component updater successfully loading the model.
  launcher.controller()->MaybeUpdateModelInfo(GetTestModelInfo());

  // Now they should have both executed immediately.
  EXPECT_TRUE(future1.IsReady());
  EXPECT_TRUE(future2.IsReady());
}

TEST_F(AISemanticEmbedderServiceLauncherTest,
       WaitForModelAvailable_QueuedAndFailed) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .WillOnce([&run_loop](const std::string&,
                            component_updater::OnDemandUpdater::Priority,
                            component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::SERVICE_ERROR);
        run_loop.Quit();
      });

  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());

  // Wait for the component registration to finish and trigger OnDemandUpdate.
  run_loop.Run();

  // The callback should have been executed because of the update error.
  EXPECT_TRUE(future.IsReady());
  // Model is still not available.
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
}

class AISemanticEmbedderServiceLauncherPurgeTest
    : public AISemanticEmbedderServiceLauncherTest {
 protected:
  PrefService* local_state() {
    return TestingBrowserProcess::GetGlobal()->local_state();
  }

  void SetUserSettingEnabled(bool enabled) {
    local_state()->SetBoolean(optimization_guide::model_execution::prefs::
                                  localstate::kOnDeviceAiUserSettingsEnabled,
                              enabled);
  }

  void SetEnterprisePolicyDisallowed() {
    local_state()->SetInteger(
        optimization_guide::model_execution::prefs::localstate::
            kGenAILocalFoundationalModelEnterprisePolicySettings,
        static_cast<int>(
            optimization_guide::model_execution::prefs::
                GenAILocalFoundationalModelEnterprisePolicySettings::
                    kDisallowed));
  }

  bool IsDownloadEligible() {
    return local_state()->GetBoolean(
        optimization_guide::model_execution::prefs::localstate::
            kEmbeddingApiModelDownloadEligible);
  }
};

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       UserSettingDisabledPurgesModel) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  launcher.controller()->MaybeUpdateModelInfo(GetTestModelInfo());
  ASSERT_TRUE(launcher.controller()->IsModelAvailable());
  local_state()->SetBoolean(optimization_guide::model_execution::prefs::
                                localstate::kEmbeddingApiModelDownloadEligible,
                            true);

  EXPECT_CALL(*mock_cus_, UnregisterComponent(
                              component_updater::GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(true));

  SetUserSettingEnabled(false);

  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
  EXPECT_FALSE(IsDownloadEligible());
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       EnterprisePolicyDisallowedPurgesModel) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  launcher.controller()->MaybeUpdateModelInfo(GetTestModelInfo());
  ASSERT_TRUE(launcher.controller()->IsModelAvailable());

  EXPECT_CALL(*mock_cus_, UnregisterComponent(
                              component_updater::GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(true));

  SetEnterprisePolicyDisallowed();

  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
  EXPECT_FALSE(IsDownloadEligible());
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       ReenablingDoesNotReregister) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  // Start with the model loaded, so the reload at the end also checks that
  // the purge reset the model version: the same version must load again.
  launcher.controller()->MaybeUpdateModelInfo(GetTestModelInfo());
  ASSERT_TRUE(launcher.controller()->IsModelAvailable());
  EXPECT_CALL(*mock_cus_, UnregisterComponent(testing::_))
      .WillOnce(testing::Return(false));
  SetUserSettingEnabled(false);

  // Turning the setting back on must not download anything by itself.
  EXPECT_CALL(*mock_cus_, RegisterComponent(testing::_)).Times(0);
  SetUserSettingEnabled(true);
  // A registration would call RegisterComponent() from the reply to a
  // ThreadPool task, so let everything run before Times(0) is verified below.
  task_environment_.RunUntilIdle();  // nocheck
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
  EXPECT_FALSE(IsDownloadEligible());
  testing::Mock::VerifyAndClearExpectations(mock_cus_.get());

  // The next create() registers the component again and downloads it.
  EXPECT_CALL(*mock_cus_, RegisterComponent(testing::_))
      .WillOnce(testing::Return(true));
  base::test::TestFuture<void> update_requested;
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .WillOnce(
          [&update_requested](const std::string&,
                              component_updater::OnDemandUpdater::Priority,
                              component_updater::Callback callback) {
            std::move(callback).Run(update_client::Error::NONE);
            update_requested.SetValue();
          });
  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());
  ASSERT_TRUE(update_requested.Wait());
  EXPECT_TRUE(IsDownloadEligible());
  EXPECT_FALSE(future.IsReady());

  // The download finishes and the waiter runs.
  launcher.controller()->MaybeUpdateModelInfo(GetTestModelInfo());
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(launcher.controller()->IsModelAvailable());
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       PurgeFlushesPendingAvailabilityCallbacks) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  ASSERT_FALSE(launcher.controller()->IsModelAvailable());

  // Registration starts a download that never completes.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .WillOnce([&run_loop](const std::string&,
                            component_updater::OnDemandUpdater::Priority,
                            component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
        run_loop.Quit();
      });
  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());
  run_loop.Run();
  ASSERT_FALSE(future.IsReady());
  ASSERT_TRUE(IsDownloadEligible());

  EXPECT_CALL(*mock_cus_, UnregisterComponent(
                              component_updater::GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(true));
  SetUserSettingEnabled(false);

  // The waiter runs and observes that the model is unavailable.
  EXPECT_TRUE(future.IsReady());
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
  EXPECT_FALSE(IsDownloadEligible());
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       WaitForModelAvailableDoesNotRegisterWhenDisallowed) {
  SetUserSettingEnabled(false);
  AISemanticEmbedderServiceLauncherForTest launcher;

  EXPECT_CALL(*mock_cus_, RegisterComponent(testing::_)).Times(0);
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
  EXPECT_FALSE(IsDownloadEligible());
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       RegistrationCompletedAfterDisableDoesNotDownload) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  EXPECT_CALL(*mock_cus_, RegisterComponent(testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());
  ASSERT_TRUE(IsDownloadEligible());

  // The setting is turned off before registration completes, so the purge
  // can't unregister the component yet. The launcher unregisters it once
  // registration completes.
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_cus_, UnregisterComponent(
                              component_updater::GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(false))
      .WillOnce([&run_loop](const std::string&) {
        run_loop.Quit();
        return true;
      });
  SetUserSettingEnabled(false);
  EXPECT_TRUE(future.IsReady());

  // Registration completes and unregisters the component.
  run_loop.Run();
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
  EXPECT_FALSE(IsDownloadEligible());
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       RegistrationCompletedAfterDisableAndReenableDoesNotDownload) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  EXPECT_CALL(*mock_cus_, RegisterComponent(testing::_))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .Times(0);

  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());
  ASSERT_TRUE(IsDownloadEligible());

  // The setting is turned off and back on before registration completes. The
  // purge failed the waiting create() and cleared the eligibility pref, and
  // nothing has asked for the model since, so the launcher unregisters the
  // component instead of downloading for a create() that already failed.
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_cus_, UnregisterComponent(
                              component_updater::GetAIEmbeddingsComponentId()))
      .WillOnce(testing::Return(false))
      .WillOnce([&run_loop](const std::string&) {
        run_loop.Quit();
        return true;
      });
  SetUserSettingEnabled(false);
  EXPECT_TRUE(future.IsReady());
  SetUserSettingEnabled(true);

  run_loop.Run();
  EXPECT_FALSE(launcher.controller()->IsModelAvailable());
  EXPECT_FALSE(IsDownloadEligible());
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       UninstallWithoutLoadedModelKeepsWaiters) {
  AISemanticEmbedderServiceLauncherForTest launcher;
  AISemanticEmbedderServiceLauncher::SetForTesting(&launcher);

  // A create() is waiting for a download.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_))
      .WillOnce([&run_loop](const std::string&,
                            component_updater::OnDemandUpdater::Priority,
                            component_updater::Callback callback) {
        std::move(callback).Run(update_client::Error::NONE);
        run_loop.Quit();
      });
  base::test::TestFuture<void> future;
  launcher.WaitForModelAvailable(future.GetCallback());
  run_loop.Run();

  // Then an earlier uninstall finishes deleting files. No model is loaded, so
  // the waiter keeps waiting for the download.
  component_updater::GetAIEmbeddingsComponentInstallerPolicyForTesting()
      ->OnCustomUninstall();
  // OnCustomUninstall() posts to the UI thread. Let that task, and anything it
  // posts, run before checking that the waiter still waits.
  task_environment_.RunUntilIdle();  // nocheck
  EXPECT_FALSE(future.IsReady());

  AISemanticEmbedderServiceLauncher::SetForTesting(nullptr);
}

TEST_F(AISemanticEmbedderServiceLauncherPurgeTest,
       SingletonWatchesLocalStateOfNewBrowserProcess) {
  // Unit tests replace the browser process and its local state for each test,
  // but the singleton lives on.
  AISemanticEmbedderServiceLauncher::Get();
  mock_cus_ = nullptr;  // Owned by the browser process being replaced.
  TestingBrowserProcess::DeleteInstance();
  TestingBrowserProcess::CreateInstance();
  SetUserSettingEnabled(true);
  local_state()->SetBoolean(optimization_guide::model_execution::prefs::
                                localstate::kEmbeddingApiModelDownloadEligible,
                            true);

  AISemanticEmbedderServiceLauncher::Get();
  SetUserSettingEnabled(false);

  EXPECT_FALSE(IsDownloadEligible());
}
