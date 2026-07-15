// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_semantic_embedder_service_launcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
  launcher.controller()->MaybeUpdateModelInfo(
      passage_embeddings::GetBuilderWithValidModelInfo().Build());
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

  EXPECT_CALL(mock_on_demand_updater_,
              OnDemandUpdate(testing::_, testing::_, testing::_));

  // Call WaitForModelAvailable multiple times, they should all be queued.
  base::test::TestFuture<void> future1;
  base::test::TestFuture<void> future2;
  launcher.WaitForModelAvailable(future1.GetCallback());
  launcher.WaitForModelAvailable(future2.GetCallback());

  // They should not have executed yet.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // Simulate component updater successfully loading the model.
  launcher.controller()->MaybeUpdateModelInfo(
      passage_embeddings::GetBuilderWithValidModelInfo().Build());

  // Now they should have both executed immediately.
  EXPECT_TRUE(future1.IsReady());
  EXPECT_TRUE(future2.IsReady());
}
