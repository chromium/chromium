// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/core/buildflags.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/states.h"
#if BUILDFLAG(ENABLE_TTC_WEBUI)
#include "chrome/browser/ttc/core/eval_harness.mojom.h"
#include "chrome/browser/ttc/core/ttc_eval_harness.h"
#endif
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

class MockSessionControllerObserver : public SessionController::Observer {
 public:
  MOCK_METHOD(void, OnSessionLifecycleChanged, (SessionLifecycle), (override));
  MOCK_METHOD(void, OnModelTurnStarted, (), (override));
  MOCK_METHOD(void, OnModelTurnComplete, (), (override));
  MOCK_METHOD(void, OnModelInterrupted, (), (override));
  MOCK_METHOD(void, OnError, (ErrorCode), (override));
  MOCK_METHOD(void,
              OnToolExecutionCompleted,
              (const std::string&, bool),
              (override));
  MOCK_METHOD(void, OnInputTranscription, (const std::string&), (override));
  MOCK_METHOD(void, OnOutputTranscription, (const std::string&), (override));
  MOCK_METHOD(void,
              OnModelAudioEnvelopeReady,
              (const AudioEnvelope&),
              (override));
};

class TtcSessionControllerImplTest : public testing::Test {
 public:
  TtcSessionControllerImplTest() {
    scoped_feature_list_.InitAndEnableFeature(kTtc);
    service_ = static_cast<TtcKeyedService*>(
        TtcKeyedServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_,
            base::BindRepeating(&TtcSessionControllerImplTest::CreateService,
                                base::Unretained(this))));
  }
  ~TtcSessionControllerImplTest() override = default;

  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    return std::make_unique<TtcKeyedService>(
        Profile::FromBrowserContext(context),
        base::BindRepeating(
            &TtcSessionControllerImplTest::CreateMockConversation,
            base::Unretained(this)));
  }

  std::unique_ptr<Conversation> CreateMockConversation(
      SessionController&,
      std::unique_ptr<AudioController> audio_controller) {
    last_audio_controller_ = std::move(audio_controller);
    auto conversation = std::make_unique<testing::NiceMock<MockConversation>>();
    mock_conversation_ = conversation.get();
    return conversation;
  }

  void TearDown() override {
    if (service_) {
      service_->EndSession();
    }
    last_audio_controller_.reset();
    mock_conversation_ = nullptr;
    testing::Test::TearDown();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<TtcKeyedService> service_ = nullptr;
  raw_ptr<MockConversation, DisableDanglingPtrDetection> mock_conversation_ =
      nullptr;
  std::unique_ptr<AudioController> last_audio_controller_;
};

TEST_F(TtcSessionControllerImplTest,
       ObserverNotifiedOfInitialStateAndTurnEvents) {
  SessionControllerImpl controller(*service_);
  EXPECT_EQ(&controller.conversation(), mock_conversation_.get());

  MockSessionControllerObserver observer;
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  controller.AddObserver(&observer);

  EXPECT_CALL(observer, OnModelTurnStarted()).Times(1);
  controller.OnModelTurnStarted();

  EXPECT_CALL(observer, OnModelTurnComplete()).Times(1);
  controller.OnModelTurnComplete();

  EXPECT_CALL(observer, OnModelInterrupted()).Times(1);
  controller.OnModelInterrupted();

  EXPECT_CALL(observer, OnInputTranscription("hello")).Times(1);
  controller.OnInputTranscription("hello");

  EXPECT_CALL(observer, OnOutputTranscription("world")).Times(1);
  controller.OnOutputTranscription("world");

  EXPECT_CALL(observer, OnToolExecutionCompleted("unknown_tool", false))
      .Times(1);
  EXPECT_CALL(observer, OnError(ErrorCode::kInternalBackendError)).Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);

  ToolRequest req;
  req.name = "unknown_tool";
  bool callback_called = false;
  controller.ProcessToolCall(req, base::BindOnce(
                                      [](bool* called, ToolResponse response) {
                                        *called = true;
                                        EXPECT_FALSE(response.Ok());
                                      },
                                      &callback_called));
  EXPECT_TRUE(callback_called);

  controller.OnError(ErrorCode::kInternalBackendError);
  EXPECT_EQ(controller.GetSessionLifecycle(), SessionLifecycle::kFinished);

  controller.RemoveObserver(&observer);
}

TEST_F(TtcSessionControllerImplTest, SendTextInputForwardsToConversation) {
  SessionControllerImpl controller(*service_);
  ASSERT_NE(mock_conversation_, nullptr);
  EXPECT_CALL(*mock_conversation_, SendTextInput("Test text input")).Times(1);
  controller.SendTextInput("Test text input");
}

TEST_F(TtcSessionControllerImplTest, DispatchesAudioEnvelopeToObservers) {
  SessionControllerImpl controller(*service_);
  testing::NiceMock<MockSessionControllerObserver> observer;
  controller.AddObserver(&observer);

  // 120ms of 440Hz sine wave at 24kHz (2880 samples = 6 slices of 20ms)
  // followed by 40ms of silence (960 samples = 2 slices of 20ms).
  std::vector<int16_t> pcm_samples(2880 + 960, 0);
  for (size_t i = 0; i < 2880; ++i) {
    pcm_samples[i] = static_cast<int16_t>(
        16000.0 *
        std::sin(2.0 * std::numbers::pi_v<double> * 440.0 * i / 24000.0));
  }

  AudioEnvelopeExtractor extractor;
  const AudioEnvelope envelope = extractor.Extract(pcm_samples);

  AudioEnvelope captured_envelope;
  EXPECT_CALL(observer, OnModelAudioEnvelopeReady(testing::_))
      .WillOnce(testing::SaveArg<0>(&captured_envelope));

  controller.OnModelAudioEnvelopeReady(envelope);

  EXPECT_GT(captured_envelope.rms, 0.3f);
  EXPECT_GT(captured_envelope.peak_amplitude, 0.45f);
  EXPECT_GT(captured_envelope.zero_crossing_rate, 0.0f);
  ASSERT_EQ(captured_envelope.segments.size(), 2u);
  EXPECT_EQ(captured_envelope.segments[0].start_time, base::Milliseconds(0));
  EXPECT_EQ(captured_envelope.segments[0].duration, base::Milliseconds(120));
  EXPECT_GT(captured_envelope.segments[0].energy_rms, 0.3f);
  EXPECT_GT(captured_envelope.segments[0].peak_amplitude, 0.45f);
  EXPECT_EQ(captured_envelope.segments[1].start_time, base::Milliseconds(120));
  EXPECT_EQ(captured_envelope.segments[1].duration, base::Milliseconds(40));
  EXPECT_FLOAT_EQ(captured_envelope.segments[1].energy_rms, 0.0f);

  controller.RemoveObserver(&observer);
}

#if BUILDFLAG(ENABLE_TTC_WEBUI)
TEST_F(TtcSessionControllerImplTest, TtcEvalHarnessStartSessionAndExecuteTurn) {
  TtcEvalHarness harness(*service_);

  base::test::TestFuture<bool> start_future;
  harness.StartSession(start_future.GetCallback());
  EXPECT_TRUE(service_->is_session_active());

  // Set session to kLive
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  EXPECT_TRUE(start_future.Get());
  ASSERT_NE(harness.audio_controller(), nullptr);

  // Execute turn with audio input
  std::vector<int16_t> samples(1600, 42);
  base::test::TestFuture<mojom::TurnResultPtr> turn_future;
  harness.SendAudio(samples, turn_future.GetCallback());

  // Simulate turn events observed from SessionController, including partial
  // transcriptions that should be concatenated.
  service_->session_controller()->OnModelTurnStarted();
  service_->session_controller()->OnInputTranscription("user ");
  service_->session_controller()->OnInputTranscription("voice");
  service_->session_controller()->OnOutputTranscription("model ");
  service_->session_controller()->OnOutputTranscription("voice");
  harness.OnToolExecutionCompleted("navigate", true);
  service_->session_controller()->OnModelTurnComplete();

  mojom::TurnResultPtr result = turn_future.Take();
  EXPECT_EQ(result->status, mojom::TurnStatus::kComplete);
  EXPECT_EQ(result->error_code, 0);
  EXPECT_EQ(result->input_transcription, "user voice");
  EXPECT_EQ(result->transcription, "model voice");
  ASSERT_EQ(result->tool_calls.size(), 1u);
  EXPECT_EQ(result->tool_calls[0]->name, "navigate");

  mock_conversation_ = nullptr;
  harness.EndSession();
}

TEST_F(TtcSessionControllerImplTest,
       TtcEvalHarnessRejectsEmptyInputOrInactiveSession) {
  TtcEvalHarness harness(*service_);

  // Sending text before session is live fails immediately.
  base::test::TestFuture<mojom::TurnResultPtr> inactive_future;
  harness.SendText("hello", inactive_future.GetCallback());
  EXPECT_EQ(inactive_future.Get()->status, mojom::TurnStatus::kError);

  base::test::TestFuture<bool> start_future;
  harness.StartSession(start_future.GetCallback());
  service_->session_controller()->SetSessionLifecycle(SessionLifecycle::kLive);
  EXPECT_TRUE(start_future.Get());

  // Empty text fails immediately without timing out.
  base::test::TestFuture<mojom::TurnResultPtr> empty_text_future;
  harness.SendText("", empty_text_future.GetCallback());
  EXPECT_EQ(empty_text_future.Get()->status, mojom::TurnStatus::kError);

  // Empty audio fails immediately without timing out.
  base::test::TestFuture<mojom::TurnResultPtr> empty_audio_future;
  harness.SendAudio({}, empty_audio_future.GetCallback());
  EXPECT_EQ(empty_audio_future.Get()->status, mojom::TurnStatus::kError);

  harness.EndSession();
}
#endif  // BUILDFLAG(ENABLE_TTC_WEBUI)

TEST_F(TtcSessionControllerImplTest, SetMicMuted) {
  SessionControllerImpl controller(*service_);
  controller.SetSessionLifecycle(SessionLifecycle::kLive);
  bool muted = false;
  ON_CALL(*mock_conversation_, IsMicMuted()).WillByDefault([&muted] {
    return muted;
  });
  EXPECT_CALL(*mock_conversation_, SetMicMuted(true))
      .WillOnce([&muted](bool value) { muted = value; });
  EXPECT_CALL(*mock_conversation_, SetMicMuted(false))
      .WillOnce([&muted](bool value) { muted = value; });

  EXPECT_TRUE(controller.SetMicMuted(true));
  EXPECT_TRUE(controller.IsMicMuted());
  // Already muted: nothing to do.
  EXPECT_FALSE(controller.SetMicMuted(true));
  EXPECT_TRUE(controller.SetMicMuted(false));
  EXPECT_FALSE(controller.IsMicMuted());
}

TEST_F(TtcSessionControllerImplTest, MicCanOnlyBeMutedOnceLive) {
  SessionControllerImpl controller(*service_);
  ON_CALL(*mock_conversation_, IsMicMuted())
      .WillByDefault(testing::Return(true));

  // Capture hasn't started yet, but that isn't muting.
  EXPECT_FALSE(controller.IsMicMuted());
  EXPECT_CALL(*mock_conversation_, SetMicMuted(testing::_)).Times(0);
  EXPECT_FALSE(controller.SetMicMuted(true));
}

TEST_F(TtcSessionControllerImplTest, FinishedSessionIgnoresMicMute) {
  SessionControllerImpl controller(*service_);

  // A fatal error ends the session.
  controller.OnError(ErrorCode::kInternalBackendError);
  ASSERT_EQ(controller.GetSessionLifecycle(), SessionLifecycle::kFinished);

  EXPECT_CALL(*mock_conversation_, SetMicMuted(testing::_)).Times(0);
  EXPECT_FALSE(controller.SetMicMuted(true));
}

}  // namespace ttc
