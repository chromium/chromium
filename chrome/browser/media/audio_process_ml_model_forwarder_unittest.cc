// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/audio_process_ml_model_forwarder.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_file.h"
#include "base/test/run_until.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/common/pref_names.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/ml_model_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace {

class MockMlModelManager : public audio::mojom::MlModelManager {
 public:
  MockMlModelManager() = default;
  ~MockMlModelManager() override = default;
  MOCK_METHOD(void,
              SetModel,
              (audio::mojom::MlModelType model_type, base::File model_file),
              (override));
  MOCK_METHOD(void,
              StopServingModel,
              (audio::mojom::MlModelType model_type),
              (override));

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
    local_state_.registry()->RegisterTimePref(
        prefs::kAudioInputStreamLastTimeCreated, base::Time());
    forwarder_ = AudioProcessMlModelForwarder::
        CreateWithoutAudioProcessObserverForTesting(&local_state_);
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

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  OptimizationGuideLogger logger_;
  TestingPrefServiceSimple local_state_;
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
  forwarder_->OnAudioCaptureStarted();

  EXPECT_TRUE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

TEST_F(AudioProcessMlModelForwarderTest,
       ObserverRegisteredImmediatelyIfTriggerEventWasRecentlyObserved) {
  // Set the pref to a time within the last 30 days.
  local_state_.SetTime(prefs::kAudioInputStreamLastTimeCreated,
                       base::Time::Now() - base::Days(15));

  // The model observer should be registered immediately on initialization
  // since the trigger event was already observed recently.
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  EXPECT_TRUE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

TEST_F(AudioProcessMlModelForwarderTest,
       ObserverNotRegisteredImmediatelyIfTriggerEventIsTooOld) {
  // Set the pref to a time larger than 30 days ago.
  local_state_.SetTime(prefs::kAudioInputStreamLastTimeCreated,
                       base::Time::Now() - base::Days(35));

  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  EXPECT_FALSE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

TEST_F(AudioProcessMlModelForwarderTest, OnAudioCaptureStartedSavesEventTime) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  // Advance time to have a known baseline.
  task_environment_.AdvanceClock(base::Days(1));
  base::Time expected_time = base::Time::Now();

  forwarder_->OnAudioCaptureStarted();

  EXPECT_EQ(local_state_.GetTime(prefs::kAudioInputStreamLastTimeCreated),
            expected_time);
}

TEST_F(AudioProcessMlModelForwarderTest,
       OnAudioCaptureStartedUpdatesSavesEventTimeAgain) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));

  // The model observer should NOT be registered yet.
  EXPECT_FALSE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));

  // Advance time.
  task_environment_.AdvanceClock(base::Days(1));
  base::Time first_capture_time = base::Time::Now();

  // Trigger audio capture via the dispatcher, to verify that the internal
  // observer is active.
  MediaCaptureDevicesDispatcher::GetInstance()->OnMediaRequestStateChanged(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*page_request_id=*/0,
      GURL(), blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      content::MEDIA_REQUEST_STATE_DONE);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return local_state_.GetTime(prefs::kAudioInputStreamLastTimeCreated) ==
           first_capture_time;
  }));

  // The model observer should now be registered.
  EXPECT_TRUE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));

  // Advance time again.
  task_environment_.AdvanceClock(base::Days(1));
  base::Time second_capture_time = base::Time::Now();

  // Trigger another audio capture.
  MediaCaptureDevicesDispatcher::GetInstance()->OnMediaRequestStateChanged(
      /*render_process_id=*/0, /*render_frame_id=*/0, /*page_request_id=*/0,
      GURL(), blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE,
      content::MEDIA_REQUEST_STATE_DONE);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return local_state_.GetTime(prefs::kAudioInputStreamLastTimeCreated) ==
           second_capture_time;
  }));
}

TEST_F(AudioProcessMlModelForwarderTest,
       DeregisterFromModelUpdatesOnDestruction) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));
  forwarder_->OnAudioCaptureStarted();
  forwarder_.reset();

  EXPECT_FALSE(model_provider_.IsRegistered(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR));
}

TEST_F(AudioProcessMlModelForwarderTest, ForwardUpdates) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));
  forwarder_->OnAudioCaptureStarted();

  testing::InSequence s;

  // Forward a model file.
  EXPECT_CALL(
      ml_model_manager_,
      SetModel(audio::mojom::MlModelType::kResidualEchoEstimation, testing::_))
      .Times(1)
      .WillOnce([](audio::mojom::MlModelType, base::File file) {
        ASSERT_TRUE(file.IsValid());
      });
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      CreateModelInfo());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));

  // Forward "stop serving" signal.
  EXPECT_CALL(
      ml_model_manager_,
      StopServingModel(audio::mojom::MlModelType::kResidualEchoEstimation))
      .Times(1);
  model_provider_.RemoveModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasModelForTesting(); }));

  // Forward another model file.
  EXPECT_CALL(
      ml_model_manager_,
      SetModel(audio::mojom::MlModelType::kResidualEchoEstimation, testing::_))
      .Times(1)
      .WillOnce([](audio::mojom::MlModelType, base::File file) {
        ASSERT_TRUE(file.IsValid());
      });
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
  forwarder_->OnAudioCaptureStarted();

  // Forward the model to the first audio process instance.
  EXPECT_CALL(
      ml_model_manager_,
      SetModel(audio::mojom::MlModelType::kResidualEchoEstimation, testing::_))
      .Times(1)
      .WillOnce([](audio::mojom::MlModelType, base::File file) {
        ASSERT_TRUE(file.IsValid());
      });
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
  EXPECT_CALL(
      ml_model_manager_2,
      SetModel(audio::mojom::MlModelType::kResidualEchoEstimation, testing::_))
      .Times(1)
      .WillOnce([](audio::mojom::MlModelType, base::File file) {
        ASSERT_TRUE(file.IsValid());
      });

  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_2));
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));
}

TEST_F(AudioProcessMlModelForwarderTest,
       HandleModelUpdateAfterAudioProcessCrash) {
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));
  forwarder_->OnAudioCaptureStarted();

  // Simulate a crash by invalidating the receiver.
  ml_model_manager_.ResetReceiver();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasBoundAudioProcessRemoteForTesting(); }));

  // Nothing should happen when the receiver has been disconnected.
  EXPECT_CALL(ml_model_manager_, SetModel(testing::_, testing::_)).Times(0);
  EXPECT_CALL(ml_model_manager_, StopServingModel(testing::_)).Times(0);

  // Send a model update with a new model.
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      CreateModelInfo());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasPendingTasksForTesting(); }));
}

TEST_F(AudioProcessMlModelForwarderTest,
       HandleStopServingSignalAfterAudioProcessCrash) {
  // Set up the forwarder with a model file.
  forwarder_->Initialize(model_provider_);
  forwarder_->OnAudioProcessLaunched(std::move(remote_ml_model_manager_));
  forwarder_->OnAudioCaptureStarted();
  model_provider_.UpdateModelImmediatelyForTesting(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR,
      CreateModelInfo());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return forwarder_->HasModelForTesting(); }));
  testing::Mock::VerifyAndClearExpectations(&ml_model_manager_);

  // Simulate a crash by invalidating the receiver.
  ml_model_manager_.ResetReceiver();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasBoundAudioProcessRemoteForTesting(); }));

  // Nothing should happen when the receiver has been disconnected.
  EXPECT_CALL(ml_model_manager_, SetModel(testing::_, testing::_)).Times(0);
  EXPECT_CALL(ml_model_manager_, StopServingModel(testing::_)).Times(0);

  // Send a model update to stop serving models.
  model_provider_.RemoveModel(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_WEBRTC_NEURAL_RESIDUAL_ECHO_ESTIMATOR);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !forwarder_->HasModelForTesting(); }));
}

}  // namespace
