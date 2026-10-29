// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/buildflags.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/session_view.h"
#include "chrome/browser/ttc/core/session_view_delegate.h"
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
#include "components/ttc/app/public/conversation.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_jni.h"
#endif

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
  }

  void SetUp() override {
    testing::Test::SetUp();
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
            base::Unretained(this)),
        session_view_factory_);
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
  // Used by the service, if set before SetUp(), instead of the platform's.
  TtcKeyedService::SessionViewFactory session_view_factory_;
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

TEST_F(TtcSessionControllerImplTest, SessionTimesOutAfterInactivity) {
  service_->StartSession();
  EXPECT_TRUE(service_->is_session_active());

  MockSessionControllerObserver observer;
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  service_->session_controller()->AddObserver(&observer);

  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(service_->is_session_active());

  EXPECT_CALL(observer, OnError(ErrorCode::kInactivityTimeout)).Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);

  task_environment_.FastForwardBy(base::Minutes(1) + base::Seconds(1));
  EXPECT_FALSE(service_->is_session_active());
}

TEST_F(TtcSessionControllerImplTest, UserSpeechAudioResetsInactivityTimer) {
  service_->StartSession();
  EXPECT_TRUE(service_->is_session_active());

  MockSessionControllerObserver observer;
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  service_->session_controller()->AddObserver(&observer);

  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(service_->is_session_active());

  // User speaks (energy >= 0.05).
  service_->session_controller()->UserAudioLevelUpdate(0.1f);

  // Fast forward another 4 minutes (8 minutes total since session start).
  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(service_->is_session_active());

  EXPECT_CALL(observer, OnError(ErrorCode::kInactivityTimeout)).Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);

  // Reaching 5 minutes after speech event triggers timeout.
  task_environment_.FastForwardBy(base::Minutes(1) + base::Seconds(1));
  EXPECT_FALSE(service_->is_session_active());
}

TEST_F(TtcSessionControllerImplTest, AmbientNoiseDoesNotResetInactivityTimer) {
  service_->StartSession();
  EXPECT_TRUE(service_->is_session_active());

  MockSessionControllerObserver observer;
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  service_->session_controller()->AddObserver(&observer);

  // Deliver ambient noise (0.01 < 0.05) every minute.
  for (int i = 0; i < 4; ++i) {
    task_environment_.FastForwardBy(base::Minutes(1));
    service_->session_controller()->UserAudioLevelUpdate(0.01f);
    EXPECT_TRUE(service_->is_session_active());
  }

  EXPECT_CALL(observer, OnError(ErrorCode::kInactivityTimeout)).Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);

  // Reaching 5 minutes triggers timeout despite ambient noise.
  task_environment_.FastForwardBy(base::Minutes(1) + base::Seconds(1));
  EXPECT_FALSE(service_->is_session_active());
}

TEST_F(TtcSessionControllerImplTest, InputTranscriptionsResetInactivityTimer) {
  service_->StartSession();
  EXPECT_TRUE(service_->is_session_active());

  MockSessionControllerObserver observer;
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  service_->session_controller()->AddObserver(&observer);

  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(service_->is_session_active());

  // Input transcription resets timer.
  EXPECT_CALL(observer, OnInputTranscription("hello")).Times(1);
  service_->session_controller()->OnInputTranscription("hello");

  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(service_->is_session_active());

  EXPECT_CALL(observer, OnError(ErrorCode::kInactivityTimeout)).Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);

  task_environment_.FastForwardBy(base::Minutes(1) + base::Seconds(1));
  EXPECT_FALSE(service_->is_session_active());
}

TEST_F(TtcSessionControllerImplTest, ModelTurnPausesInactivityTimer) {
  service_->StartSession();
  EXPECT_TRUE(service_->is_session_active());

  MockSessionControllerObserver observer;
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  service_->session_controller()->AddObserver(&observer);

  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(service_->is_session_active());

  // Model begins response.
  EXPECT_CALL(observer, OnModelTurnStarted()).Times(1);
  service_->session_controller()->OnModelTurnStarted();

  // Audio energy arrives while model is speaking; timer must not restart.
  service_->session_controller()->UserAudioLevelUpdate(0.1f);

  // 3 minutes pass during model generation/speech.
  task_environment_.FastForwardBy(base::Minutes(3));
  EXPECT_TRUE(service_->is_session_active());

  // Model turn completes.
  EXPECT_CALL(observer, OnModelTurnComplete()).Times(1);
  service_->session_controller()->OnModelTurnComplete();

  // 4 minutes pass in user inactivity.
  task_environment_.FastForwardBy(base::Minutes(4));
  EXPECT_TRUE(service_->is_session_active());

  EXPECT_CALL(observer, OnError(ErrorCode::kInactivityTimeout)).Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);

  // 1 minute later (5 minutes after model turn completed), timeout fires.
  task_environment_.FastForwardBy(base::Minutes(1) + base::Seconds(1));
  EXPECT_FALSE(service_->is_session_active());
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

namespace {

// What a FakeSessionView has been told.
struct SessionViewRecord {
  std::string user_transcript;
  std::string model_transcript;
};

class FakeSessionView : public SessionView {
 public:
  explicit FakeSessionView(SessionViewRecord& record) : record_(record) {}
  ~FakeSessionView() override = default;

  // SessionView:
  void UpdateAudioLevel(float audio_level) override {}
  void OnSessionInitialized() override {}
  void OnError(ErrorCode error) override {}
  void OnTranscriptionsUpdated(const std::string& user_transcript,
                               const std::string& model_transcript) override {
    record_->user_transcript = user_transcript;
    record_->model_transcript = model_transcript;
  }

 private:
  const raw_ref<SessionViewRecord> record_;
};

}  // namespace

// Checks what the session UI is told, through a FakeSessionView.
class TtcSessionControllerImplViewTest : public TtcSessionControllerImplTest {
 protected:
  TtcSessionControllerImplViewTest() {
    session_view_factory_ =
        base::BindRepeating(&TtcSessionControllerImplViewTest::CreateFakeView,
                            base::Unretained(this));
  }

  std::unique_ptr<SessionView> CreateFakeView(SessionViewDelegate&) {
    return std::make_unique<FakeSessionView>(record_);
  }

  SessionViewRecord record_;
};

// How exchanges are split is covered by TranscriptTracker's tests; this checks
// that the transcripts reach the session UI.
TEST_F(TtcSessionControllerImplViewTest, TranscriptsReachView) {
  SessionControllerImpl controller(*service_);

  controller.OnInputTranscription("What's the weather?");
  controller.OnOutputTranscription("It's sunny.");

  EXPECT_EQ(record_.user_transcript, "What's the weather?");
  EXPECT_EQ(record_.model_transcript, "It's sunny.");
}

TEST_F(TtcSessionControllerImplViewTest, FinishedSessionStopsUpdatingView) {
  SessionControllerImpl controller(*service_);
  controller.OnInputTranscription("Hello");

  // A fatal error ends the session.
  controller.OnError(ErrorCode::kInternalBackendError);
  ASSERT_EQ(controller.GetSessionLifecycle(), SessionLifecycle::kFinished);

  controller.OnOutputTranscription("Hi");
  EXPECT_EQ(record_.user_transcript, "Hello");
  EXPECT_EQ(record_.model_transcript, "");
}

#if BUILDFLAG(IS_ANDROID)
// Creates Android browser windows that register with GlobalBrowserCollection
// the way ChromeAndroidTaskTracker does in production, and destroys them all on
// destruction. Windows are identified by the Android task id they live in.
class ScopedAndroidBrowserWindows {
 public:
  ScopedAndroidBrowserWindows() = default;
  ScopedAndroidBrowserWindows(const ScopedAndroidBrowserWindows&) = delete;
  ScopedAndroidBrowserWindows& operator=(const ScopedAndroidBrowserWindows&) =
      delete;
  ~ScopedAndroidBrowserWindows() {
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyAllBrowserWindows(
        base::android::AttachCurrentThread());
  }

  BrowserWindowInterface* Create(Profile& profile, int task_id) {
    return reinterpret_cast<BrowserWindowInterface*>(
        Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_createBrowserWindow(
            base::android::AttachCurrentThread(), task_id,
            profile.GetJavaObject()));
  }

  void Destroy(int task_id) {
    Java_BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_destroyBrowserWindow(
        base::android::AttachCurrentThread(), task_id);
  }
};

TEST_F(TtcSessionControllerImplTest, GetBrowserWindowInterfaceNoWindows) {
  SessionControllerImpl controller(*service_);
  EXPECT_EQ(controller.GetBrowserWindowInterface(), nullptr);
}

TEST_F(TtcSessionControllerImplTest,
       GetBrowserWindowInterfaceFollowsProfileWindows) {
  ScopedAndroidBrowserWindows windows;

  // A window that already exists when the session starts.
  BrowserWindowInterface* window1 = windows.Create(profile_, /*task_id=*/1);
  ASSERT_TRUE(window1);
  SessionControllerImpl controller(*service_);
  EXPECT_EQ(controller.GetBrowserWindowInterface(), window1);

  // A newly created window is not the last active one.
  BrowserWindowInterface* window2 = windows.Create(profile_, /*task_id=*/2);
  ASSERT_TRUE(window2);
  EXPECT_EQ(controller.GetBrowserWindowInterface(), window1);

  // Closing the current window falls back to the remaining one.
  windows.Destroy(/*task_id=*/1);
  EXPECT_EQ(controller.GetBrowserWindowInterface(), window2);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace ttc
