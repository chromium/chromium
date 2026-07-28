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
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/passage_embeddings_model_metadata.pb.h"
#include "components/update_client/update_client_errors.h"
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
  }

  void TearDown() override {
    mock_cus_ = nullptr;
    TestingBrowserProcess::GetGlobal()->SetComponentUpdater(nullptr);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
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
