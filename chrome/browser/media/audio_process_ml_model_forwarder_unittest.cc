// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_process_ml_model_forwarder.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_file.h"
#include "base/test/run_until.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockMlModelManager : public audio::mojom::MlModelManager {
 public:
  MockMlModelManager() = default;
  ~MockMlModelManager() override = default;
  MOCK_METHOD(void,
              SetResidualEchoEstimationModel,
              (base::File model_file),
              (override));
  MOCK_METHOD(void, StopServingResidualEchoEstimationModel, (), (override));

  void BindReceiver(
      mojo::PendingReceiver<audio::mojom::MlModelManager> pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }

  void ResetReceiver() { receiver_.reset(); }

 private:
  mojo::Receiver<audio::mojom::MlModelManager> receiver_{this};
};

class AudioProcessMlModelForwarderTest : public testing::Test {
 protected:
  void SetUp() override {
    forwarder_ = AudioProcessMlModelForwarder::
        CreateWithoutAudioProcessObserverForTesting();
    ml_model_manager_.BindReceiver(
        remote_ml_model_manager_.BindNewPipeAndPassReceiver());
  }

  std::unique_ptr<optimization_guide::ModelInfo> CreateModelInfo() {
    auto temp_file = std::make_unique<base::ScopedTempFile>();
    CHECK(temp_file->Create());
    model_files_.push_back(std::move(temp_file));
    return optimization_guide::TestModelInfoBuilder()
        .SetModelFilePath(model_files_.back()->path())
        .Build();
  }

  mojo::Remote<audio::mojom::MlModelManager> CreateNewMlModelManager(
      MockMlModelManager* new_ml_model_manager) {
    mojo::Remote<audio::mojom::MlModelManager> new_remote;
    new_ml_model_manager->BindReceiver(new_remote.BindNewPipeAndPassReceiver());
    return new_remote;
  }

  content::BrowserTaskEnvironment task_environment_;
  OptimizationGuideLogger logger_;
  optimization_guide::ModelProviderRegistry model_provider_{&logger_};
  std::unique_ptr<AudioProcessMlModelForwarder> forwarder_;
  MockMlModelManager ml_model_manager_;
  mojo::Remote<audio::mojom::MlModelManager> remote_ml_model_manager_;

  // Keep track of temporary files so they don't get deleted before the test
  // ends.
  std::vector<std::unique_ptr<base::ScopedTempFile>> model_files_;
};

TEST_F(AudioProcessMlModelForwarderTest,
       DoNotRegisterForModelUpdatesBeforeAudioProcessLaunch) {
  forwarder_->Initialize(model_provider_);

  EXPECT_FALSE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

TEST_F(AudioProcessMlModelForwarderTest,
       DoNotRegisterForModelUpdatesBeforeInitialization) {
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  EXPECT_FALSE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

TEST_F(AudioProcessMlModelForwarderTest,
       RegisterForModelUpdatesAfterInitializationAndAudioProcessLaunch) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  EXPECT_TRUE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

TEST_F(AudioProcessMlModelForwarderTest,
       DeregisterFromModelUpdatesOnDestruction) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));
  forwarder_.reset();

  EXPECT_FALSE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

// TODO(crbug.com/464181367): Fix and re-enable the test.
TEST_F(AudioProcessMlModelForwarderTest, DISABLED_ForwardUpdates) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  testing::InSequence s;

  // Forward a model file.
  EXPECT_CALL(ml_model_manager_, SetResidualEchoEstimationModel(testing::_))
      .Times(1)
      .WillOnce([](base::File file) { ASSERT_TRUE(file.IsValid()); });
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      CreateModelInfo());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));

  // Forward "stop serving" signal.
  EXPECT_CALL(ml_model_manager_, StopServingResidualEchoEstimationModel())
      .Times(1);
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      nullptr);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));

  // Forward another model file.
  EXPECT_CALL(ml_model_manager_, SetResidualEchoEstimationModel(testing::_))
      .Times(1)
      .WillOnce([](base::File file) { ASSERT_TRUE(file.IsValid()); });
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      CreateModelInfo());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));
}

TEST_F(AudioProcessMlModelForwarderTest,
       ForwardModelFileOnAudioProcessRestart) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  // Forward the model to the first audio process instance.
  EXPECT_CALL(ml_model_manager_, SetResidualEchoEstimationModel(testing::_))
      .Times(1)
      .WillOnce([](base::File file) { ASSERT_TRUE(file.IsValid()); });
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      CreateModelInfo());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));
  testing::Mock::VerifyAndClearExpectations(&ml_model_manager_);

  // Forward the model to the second audio process instance.
  MockMlModelManager ml_model_manager_2;
  mojo::Remote<audio::mojom::MlModelManager> remote_ml_model_manager_2 =
      CreateNewMlModelManager(&ml_model_manager_2);
  EXPECT_CALL(ml_model_manager_2, SetResidualEchoEstimationModel(testing::_))
      .Times(1)
      .WillOnce([](base::File file) { ASSERT_TRUE(file.IsValid()); });

  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_2));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));
}

TEST_F(AudioProcessMlModelForwarderTest,
       HandleModelUpdateAfterAudioProcessCrash) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  // Simulate a crash by invalidating the receiver.
  ml_model_manager_.ResetReceiver();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasBoundAudioProcessRemoteForTesting(); }));

  // Nothing should happen when the receiver has been disconnected.
  EXPECT_CALL(ml_model_manager_, SetResidualEchoEstimationModel(testing::_))
      .Times(0);
  EXPECT_CALL(ml_model_manager_, StopServingResidualEchoEstimationModel())
      .Times(0);

  // Send a model update with a new model.
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      CreateModelInfo());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));
}

// TODO(crbug.com/464181367): Fix and re-enable the test.
TEST_F(AudioProcessMlModelForwarderTest,
       DISABLED_HandleStopServingSignalAfterAudioProcessCrash) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  // Simulate a crash by invalidating the receiver.
  ml_model_manager_.ResetReceiver();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasBoundAudioProcessRemoteForTesting(); }));

  // Nothing should happen when the receiver has been disconnected.
  EXPECT_CALL(ml_model_manager_, SetResidualEchoEstimationModel(testing::_))
      .Times(0);
  EXPECT_CALL(ml_model_manager_, StopServingResidualEchoEstimationModel())
      .Times(0);

  // Send a model update to stop serving models.
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      nullptr);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));
}

}  // namespace
