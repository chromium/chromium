// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/session_controller_impl.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/critical_actions/critical_action_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/public/audio_controller.h"
#include "chrome/browser/ttc/core/buildflags.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/output_volume_monitor.h"
#include "chrome/browser/ttc/core/pointer_tracker.h"
#include "chrome/browser/ttc/core/states.h"
#if BUILDFLAG(ENABLE_TTC_WEBUI)
#include "chrome/browser/ttc/core/eval_harness.mojom.h"
#include "chrome/browser/ttc/core/ttc_eval_harness.h"
#endif
#include "chrome/browser/ttc/core/test_utils.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
#include "chrome/browser/ttc/debug/ttc_debug_session.h"
#include "chrome/browser/ttc/debug/ttc_debug_tools_service.h"
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
#include "chrome/test/base/testing_profile.h"
#include "components/critical_actions/core/browser/critical_action_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ttc/app/public/conversation.h"
#include "components/ttc/app/public/debug_sink.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/native_unit_test_support_jni/BrowserWindowInterfaceIteratorAndroidNativeUnitTestSupport_jni.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/run_until.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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
              (const std::string&, const base::DictValue&, bool),
              (override));
  MOCK_METHOD(void, OnInputTranscription, (const std::string&), (override));
  MOCK_METHOD(void, OnOutputTranscription, (const std::string&), (override));
  MOCK_METHOD(void,
              OnTranscriptionsUpdated,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              OnModelAudioEnvelopeReady,
              (const AudioEnvelope&),
              (override));
  MOCK_METHOD(void, OnMicMutedChanged, (bool), (override));
  MOCK_METHOD(void, OnSpeakerChanged, (Speaker), (override));
  MOCK_METHOD(void, OnMicCaptureChanged, (bool), (override));
  MOCK_METHOD(void, OnSystemMicMuteChanged, (bool, bool), (override));
  MOCK_METHOD(void,
              OnPointerLocationsReported,
              (base::span<const gfx::Point>),
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
  base::test::ScopedPowerMonitorTestSource power_monitor_source_;
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

  const gfx::Point points[] = {gfx::Point(3, 4), gfx::Point(5, 6)};
  EXPECT_CALL(observer, OnPointerLocationsReported(testing::ElementsAre(
                            gfx::Point(3, 4), gfx::Point(5, 6))))
      .Times(1);
  controller.NotifyPointerLocationsReported(points);

  EXPECT_CALL(observer, OnInputTranscription("hello")).Times(1);
  EXPECT_CALL(observer, OnTranscriptionsUpdated("hello", "")).Times(1);
  controller.OnInputTranscription("hello");

  EXPECT_CALL(observer, OnOutputTranscription("world")).Times(1);
  EXPECT_CALL(observer, OnTranscriptionsUpdated("hello", "world")).Times(1);
  controller.OnOutputTranscription("world");

  EXPECT_CALL(observer,
              OnToolExecutionCompleted("unknown_tool", testing::_, false))
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

#if BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)
// With the debug tools off (the default), nothing is created for them and the
// pipeline gets no sink to report into.
TEST_F(TtcSessionControllerImplTest, NoDebugSinkWhenDebugToolsDisabled) {
  ASSERT_FALSE(base::FeatureList::IsEnabled(kTtcDebugTools));
  SessionControllerImpl controller(*service_);
  EXPECT_EQ(controller.GetDebugSink(), nullptr);
  EXPECT_EQ(TtcDebugToolsService::Get(&profile_), nullptr);

  // Reports that would go to the debug tools are skipped.
  controller.OnError(ErrorCode::kInternalBackendError);
}

TEST_F(TtcSessionControllerImplTest, DebugSinkWhenDebugToolsEnabled) {
  base::test::ScopedFeatureList debug_tools_feature(kTtcDebugTools);
  auto controller = std::make_unique<SessionControllerImpl>(*service_);
  TtcDebugToolsService* debug_tools = TtcDebugToolsService::Get(&profile_);
  ASSERT_NE(debug_tools, nullptr);
  TtcDebugSession* session = debug_tools->current_session();
  ASSERT_NE(session, nullptr);
  // The pipeline reports straight into the record.
  DebugSink* debug_sink = controller->GetDebugSink();
  ASSERT_EQ(debug_sink, session);

  // Session errors reach the record.
  controller->OnError(ErrorCode::kInternalBackendError);
  EXPECT_EQ(session->GetSnapshot().last_error,
            base::ToString(ErrorCode::kInternalBackendError));

  // A tool call opens a turn. When the session ends, the record closes the
  // turn and logs it to the session journal, which still exists then.
  actor::ActorJournalRecorder journal_recorder(
      CHECK_DEREF(actor::ActorKeyedService::Get(&profile_)).GetJournal());
  debug_sink->OnServerFrame(DebugSink::ToolCall{
      .call_id = "call-1", .name = "navigate", .arguments_json = "{}"});
  EXPECT_TRUE(journal_recorder.GetEntries("TtcDebugTurn").empty());
  controller.reset();
  ASSERT_EQ(debug_tools->current_session(), session);
  EXPECT_TRUE(session->ended());
  EXPECT_EQ(journal_recorder.GetEntries("TtcDebugTurn").size(), 1u);
}
#endif  // BUILDFLAG(ENABLE_TTC_DEBUG_TOOLS)

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

// Suspending the device ends the session, without the error toast an
// inactivity timeout shows.
TEST_F(TtcSessionControllerImplTest, SuspendEndsSessionWithoutError) {
  service_->StartSession();
  ASSERT_TRUE(service_->is_session_active());

  testing::NiceMock<MockSessionControllerObserver> observer;
  EXPECT_CALL(observer, OnError).Times(0);
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);
  service_->session_controller()->AddObserver(&observer);

  power_monitor_source_.GenerateSuspendEvent();
  EXPECT_FALSE(service_->is_session_active());
}

#if BUILDFLAG(IS_CHROMEOS)
class TtcSessionControllerImplUserSwitchTest
    : public TtcSessionControllerImplTest {
 protected:
  static constexpr AccountId::Literal kPrimaryAccountId =
      AccountId::Literal::FromUserEmailGaiaId("primary@example.com",
                                              GaiaId::Literal("primary-gaia"));
  static constexpr AccountId::Literal kSecondaryAccountId =
      AccountId::Literal::FromUserEmailGaiaId(
          "secondary@example.com",
          GaiaId::Literal("secondary-gaia"));

  void SetUp() override {
    TtcSessionControllerImplTest::SetUp();
    user_session_test_environment_ =
        std::make_unique<ash::test::UserSessionTestEnvironment>(
            TestingBrowserProcess::GetGlobal()->local_state());
    ASSERT_TRUE(
        user_session_test_environment_->AddRegularUser(kPrimaryAccountId));
    ASSERT_TRUE(
        user_session_test_environment_->AddRegularUser(kSecondaryAccountId));
    user_session_test_environment_->LogIn(kPrimaryAccountId);
    user_session_test_environment_->LogIn(kSecondaryAccountId);
  }

  void TearDown() override {
    TtcSessionControllerImplTest::TearDown();
    user_session_test_environment_.reset();
  }

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

TEST_F(TtcSessionControllerImplUserSwitchTest,
       UserSwitchEndsSessionWithoutError) {
  service_->StartSession();
  ASSERT_TRUE(service_->is_session_active());

  testing::NiceMock<MockSessionControllerObserver> observer;
  EXPECT_CALL(observer, OnError).Times(0);
  EXPECT_CALL(observer,
              OnSessionLifecycleChanged(SessionLifecycle::kInitializing))
      .Times(1);
  EXPECT_CALL(observer, OnSessionLifecycleChanged(SessionLifecycle::kFinished))
      .Times(1);
  service_->session_controller()->AddObserver(&observer);

  session_manager::SessionManager::Get()->SwitchActiveSession(
      kSecondaryAccountId);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return !service_->is_session_active(); }));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

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

  testing::NiceMock<MockSessionControllerObserver> observer;
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
  harness.OnToolExecutionCompleted("navigate", base::DictValue(), true);
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
  EXPECT_CALL(*mock_conversation_, SetMicMuted(true));
  EXPECT_CALL(*mock_conversation_, SetMicMuted(false));
  testing::NiceMock<MockSessionControllerObserver> observer;
  controller.AddObserver(&observer);

  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, OnMicMutedChanged(true)).Times(1);
    EXPECT_CALL(observer, OnMicMutedChanged(false)).Times(1);
  }
  EXPECT_TRUE(controller.SetMicMuted(true));
  EXPECT_TRUE(controller.IsMicMuted());
  // Already muted: nothing to do, and nothing to report.
  EXPECT_FALSE(controller.SetMicMuted(true));
  EXPECT_TRUE(controller.SetMicMuted(false));
  EXPECT_FALSE(controller.IsMicMuted());

  controller.RemoveObserver(&observer);
}

TEST_F(TtcSessionControllerImplTest, MicCanOnlyBeMutedOnceLive) {
  SessionControllerImpl controller(*service_);

  EXPECT_CALL(*mock_conversation_, SetMicMuted(testing::_)).Times(0);
  EXPECT_FALSE(controller.SetMicMuted(true));
  EXPECT_FALSE(controller.IsMicMuted());
}

TEST_F(TtcSessionControllerImplTest, FinishedSessionIgnoresMicMute) {
  SessionControllerImpl controller(*service_);

  // A fatal error ends the session.
  controller.OnError(ErrorCode::kInternalBackendError);
  ASSERT_EQ(controller.GetSessionLifecycle(), SessionLifecycle::kFinished);

  EXPECT_CALL(*mock_conversation_, SetMicMuted(testing::_)).Times(0);
  EXPECT_FALSE(controller.SetMicMuted(true));
}

TEST_F(TtcSessionControllerImplTest, FinishingSessionEndsMicMute) {
  SessionControllerImpl controller(*service_);
  controller.SetSessionLifecycle(SessionLifecycle::kLive);
  EXPECT_CALL(*mock_conversation_, SetMicMuted(true));
  ASSERT_TRUE(controller.SetMicMuted(true));
  testing::NiceMock<MockSessionControllerObserver> observer;
  controller.AddObserver(&observer);

  EXPECT_CALL(*mock_conversation_, SetMicMuted(false)).Times(0);
  EXPECT_CALL(observer, OnMicMutedChanged).Times(0);
  controller.SetSessionLifecycle(SessionLifecycle::kFinished);
  EXPECT_FALSE(controller.IsMicMuted());

  controller.RemoveObserver(&observer);
}

TEST_F(TtcSessionControllerImplTest, SpeakerChangesReachObservers) {
  SessionControllerImpl controller(*service_);
  testing::NiceMock<MockSessionControllerObserver> observer;
  controller.AddObserver(&observer);
  EXPECT_EQ(controller.GetSpeaker(), Speaker::kIdle);

  {
    testing::InSequence sequence;
    EXPECT_CALL(observer, OnSpeakerChanged(Speaker::kUser)).Times(1);
    EXPECT_CALL(observer, OnSpeakerChanged(Speaker::kModel)).Times(1);
  }
  controller.OnSpeakerChanged(Speaker::kUser);
  EXPECT_EQ(controller.GetSpeaker(), Speaker::kUser);
  // Unchanged: nothing to report.
  controller.OnSpeakerChanged(Speaker::kUser);
  controller.OnSpeakerChanged(Speaker::kModel);
  EXPECT_EQ(controller.GetSpeaker(), Speaker::kModel);

  controller.RemoveObserver(&observer);
}

TEST_F(TtcSessionControllerImplTest, FinishedSessionStopsReportingSpeaker) {
  SessionControllerImpl controller(*service_);
  testing::NiceMock<MockSessionControllerObserver> observer;
  controller.AddObserver(&observer);
  controller.SetSessionLifecycle(SessionLifecycle::kFinished);

  EXPECT_CALL(observer, OnSpeakerChanged).Times(0);
  controller.OnSpeakerChanged(Speaker::kModel);
  EXPECT_EQ(controller.GetSpeaker(), Speaker::kIdle);

  controller.RemoveObserver(&observer);
}

// The user's turn starts when they start speaking or send text; the model's
// turn leaves it alone.
TEST_F(TtcSessionControllerImplTest, UserTurnStartFollowsTheUsersTurns) {
  SessionControllerImpl controller(*service_);
  // Exact comparisons with Now() hold because the test uses mock time.
  EXPECT_EQ(controller.user_turn_start(), base::TimeTicks::Now());

  task_environment_.FastForwardBy(base::Seconds(5));
  controller.OnSpeakerChanged(Speaker::kUser);
  const base::TimeTicks first_turn = base::TimeTicks::Now();
  EXPECT_EQ(controller.user_turn_start(), first_turn);

  task_environment_.FastForwardBy(base::Seconds(5));
  controller.OnSpeakerChanged(Speaker::kModel);
  EXPECT_EQ(controller.user_turn_start(), first_turn);

  task_environment_.FastForwardBy(base::Seconds(5));
  controller.SendTextInput("what is this?");
  EXPECT_EQ(controller.user_turn_start(), base::TimeTicks::Now());
}

// The tracker exists only with the feature, and takes its thresholds from the
// feature's params.
TEST_F(TtcSessionControllerImplTest, PointerTrackerFollowsFeatureAndParams) {
  {
    SessionControllerImpl controller(*service_);
    EXPECT_FALSE(controller.pointer_tracker());
  }
  // Two clicks at different spots are two locations; how many are reported
  // depends on max_results, which is at least one however Finch sets it.
  for (const auto& [max_results, expected_count] :
       {std::pair("2", 2u), std::pair("-1", 1u)}) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        kTtcGetPointer, {{"max_results", max_results}});
    SessionControllerImpl controller(*service_);
    ASSERT_TRUE(controller.pointer_tracker());
    const base::TimeTicks now = base::TimeTicks::Now();
    controller.pointer_tracker()->RecordSample(now, gfx::PointF(10, 10),
                                               /*is_press=*/true);
    controller.pointer_tracker()->RecordSample(now, gfx::PointF(300, 300),
                                               /*is_press=*/true);
    EXPECT_EQ(controller.pointer_tracker()->ResolveLocations(now, now).size(),
              expected_count)
        << "max_results=" << max_results;
  }
}

TEST_F(TtcSessionControllerImplTest, FinishedSessionStopsUpdatingTranscripts) {
  SessionControllerImpl controller(*service_);
  testing::NiceMock<MockSessionControllerObserver> observer;
  controller.AddObserver(&observer);

  EXPECT_CALL(observer, OnTranscriptionsUpdated("Hello", ""));
  controller.OnInputTranscription("Hello");

  // A fatal error ends the session.
  controller.OnError(ErrorCode::kInternalBackendError);
  ASSERT_EQ(controller.GetSessionLifecycle(), SessionLifecycle::kFinished);

  EXPECT_CALL(observer, OnTranscriptionsUpdated).Times(0);
  controller.OnOutputTranscription("Hi");

  controller.RemoveObserver(&observer);
}

TEST_F(TtcSessionControllerImplTest, MicSignalsReachObserversUntilFinished) {
  SessionControllerImpl controller(*service_);
  testing::NiceMock<MockSessionControllerObserver> observer;
  controller.AddObserver(&observer);

  EXPECT_CALL(observer, OnMicCaptureChanged(true));
  controller.OnMicCaptureChanged(true);
  EXPECT_CALL(observer,
              OnSystemMicMuteChanged(true, /*at_stream_start=*/false));
  controller.OnSystemMicMuteChanged(true, /*at_stream_start=*/false);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // A fatal error ends the session.
  controller.OnError(ErrorCode::kInternalBackendError);
  ASSERT_EQ(controller.GetSessionLifecycle(), SessionLifecycle::kFinished);

  EXPECT_CALL(observer, OnMicCaptureChanged).Times(0);
  EXPECT_CALL(observer, OnSystemMicMuteChanged).Times(0);
  controller.OnMicCaptureChanged(false);
  controller.OnSystemMicMuteChanged(false, /*at_stream_start=*/false);

  controller.RemoveObserver(&observer);
}

class MockCriticalActionService
    : public critical_actions::CriticalActionService {
 public:
  explicit MockCriticalActionService(const base::FilePath& db_path)
      : critical_actions::CriticalActionService(
            db_path,
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                 base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}
  ~MockCriticalActionService() override = default;

  MOCK_METHOD(void,
              SetCriticalActionsConversationId,
              (const std::vector<std::string>&, std::string_view),
              (override));
};

TEST_F(TtcSessionControllerImplTest,
       SetSessionIdUpdatesTaskAndPreservesAcrossReplacement) {
  auto* mock_critical_action_service = static_cast<MockCriticalActionService*>(
      critical_actions::CriticalActionFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              &profile_,
              base::BindRepeating([](content::BrowserContext* context)
                                      -> std::unique_ptr<KeyedService> {
                return std::make_unique<
                    testing::NiceMock<MockCriticalActionService>>(
                    context->GetPath().Append(
                        FILE_PATH_LITERAL("TestCriticalActions.db")));
              })));
  ASSERT_NE(mock_critical_action_service, nullptr);

  SessionControllerImpl controller(*service_);
  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(&profile_);
  ASSERT_NE(actor_service, nullptr);

  const actor::TaskId first_task_id = controller.GetActorTaskId();
  ASSERT_FALSE(first_task_id.is_null());
  const actor::ActorTask* first_task = actor_service->GetTask(first_task_id);
  ASSERT_NE(first_task, nullptr);
  EXPECT_EQ(first_task->source_info().type,
            actor::TaskSourceInfo::Client::kTtc);
  EXPECT_EQ(first_task->source_info().id, std::nullopt);

  // Empty session ID is ignored.
  EXPECT_CALL(*mock_critical_action_service,
              SetCriticalActionsConversationId(testing::_, testing::_))
      .Times(0);
  controller.SetSessionId("");
  EXPECT_EQ(first_task->source_info().id, std::nullopt);

  // Setting a non-empty session ID updates the active ActorTask's source ID
  // and associates the task ID in CriticalActionService.
  EXPECT_CALL(
      *mock_critical_action_service,
      SetCriticalActionsConversationId(
          testing::ElementsAre(base::NumberToString(first_task_id.value())),
          std::string_view("server-session-456")))
      .Times(1);
  controller.SetSessionId("server-session-456");
  EXPECT_EQ(first_task->source_info().id, "server-session-456");

  // If the task is stopped externally and replaced, the replacement task
  // inherits the session ID and registers with CriticalActionService.
  actor_service->StopTask(first_task_id,
                          actor::ActorTask::StoppedReason::kStoppedByUser);
  EXPECT_CALL(*mock_critical_action_service,
              SetCriticalActionsConversationId(
                  testing::SizeIs(1), std::string_view("server-session-456")))
      .Times(1);
  controller.EnsureActorTaskCreated();
  const actor::TaskId second_task_id = controller.GetActorTaskId();
  EXPECT_NE(second_task_id, first_task_id);
  const actor::ActorTask* second_task = actor_service->GetTask(second_task_id);
  ASSERT_NE(second_task, nullptr);
  EXPECT_EQ(second_task->source_info().type,
            actor::TaskSourceInfo::Client::kTtc);
  EXPECT_EQ(second_task->source_info().id, "server-session-456");
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

namespace {

constexpr OutputVolumeState kAudible{.muted = false, .level = 0.5f};

}  // namespace

class TtcSessionControllerImplSpeakerVolumeTest
    : public TtcSessionControllerImplTest {
 public:
  // Stands in for the platform's monitor. Registers itself with the test while
  // it lives, so the test can change the volume.
  class FakeOutputVolumeMonitor : public OutputVolumeMonitor {
   public:
    FakeOutputVolumeMonitor(TtcSessionControllerImplSpeakerVolumeTest& test,
                            StateChangedCallback callback)
        : test_(test),
          state_(test.initial_state_),
          callback_(std::move(callback)) {
      test_->monitor_ = this;
    }
    ~FakeOutputVolumeMonitor() override { test_->monitor_ = nullptr; }

    std::optional<OutputVolumeState> GetState() const override {
      return state_;
    }

    void SetState(const OutputVolumeState& state) {
      state_ = state;
      callback_.Run(state);
    }

   private:
    const raw_ref<TtcSessionControllerImplSpeakerVolumeTest> test_;
    std::optional<OutputVolumeState> state_;
    const StateChangedCallback callback_;
  };

  TtcSessionControllerImplSpeakerVolumeTest() {
    feature_list_.InitAndEnableFeature(kTtcSpeakerVolumeWarning);
  }

 protected:
  std::unique_ptr<OutputVolumeMonitor> CreateMonitor(
      OutputVolumeMonitor::StateChangedCallback callback) {
    ++monitors_created_;
    if (!platform_supported_) {
      return nullptr;
    }
    return std::make_unique<FakeOutputVolumeMonitor>(*this,
                                                     std::move(callback));
  }

  void SetVolume(const OutputVolumeState& state) {
    ASSERT_TRUE(monitor_);
    monitor_->SetState(state);
  }

  base::test::ScopedFeatureList feature_list_;
  // What the monitor reports when a session creates it.
  std::optional<OutputVolumeState> initial_state_ = kAudible;
  // Whether the platform can read the volume, i.e. `Create()` returns one.
  bool platform_supported_ = true;
  int monitors_created_ = 0;
  // The live session's monitor, if any.
  raw_ptr<FakeOutputVolumeMonitor> monitor_ = nullptr;
  OutputVolumeMonitor::ScopedFactoryForTesting factory_{base::BindRepeating(
      &TtcSessionControllerImplSpeakerVolumeTest::CreateMonitor,
      base::Unretained(this))};
};

TEST_F(TtcSessionControllerImplSpeakerVolumeTest, NoMonitorWithoutFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kTtcSpeakerVolumeWarning);
  SessionControllerImpl controller(*service_);
  EXPECT_EQ(monitors_created_, 0);
}

// The session follows the volume for as long as it lives.
TEST_F(TtcSessionControllerImplSpeakerVolumeTest, SessionOwnsMonitor) {
  {
    SessionControllerImpl controller(*service_);
    EXPECT_EQ(monitors_created_, 1);
    ASSERT_TRUE(monitor_);
  }
  EXPECT_FALSE(monitor_);
}

TEST_F(TtcSessionControllerImplSpeakerVolumeTest, LogsVolumeToJournal) {
  actor::ActorJournalRecorder recorder(
      actor::ActorKeyedService::Get(&profile_)->GetJournal());
  SessionControllerImpl controller(*service_);
  SetVolume({.muted = true, .level = std::nullopt});

  const std::vector<actor::ActorJournalRecorder::Entry> entries =
      recorder.GetEntries("TtcSystemSpeakerVolume");
  ASSERT_EQ(entries.size(), 2u);
  // The state read when the session starts.
  EXPECT_THAT(entries[0].details,
              testing::Contains(testing::Pair("muted", "false")));
  EXPECT_THAT(entries[0].details,
              testing::Contains(testing::Pair("level", "0.5")));
  // A change on a device without a volume level.
  EXPECT_THAT(entries[1].details,
              testing::Contains(testing::Pair("muted", "true")));
  EXPECT_THAT(entries[1].details,
              testing::Not(testing::Contains(testing::Key("level"))));
}

TEST_F(TtcSessionControllerImplSpeakerVolumeTest,
       UnknownInitialVolumeIsNotLogged) {
  initial_state_ = std::nullopt;
  actor::ActorJournalRecorder recorder(
      actor::ActorKeyedService::Get(&profile_)->GetJournal());
  SessionControllerImpl controller(*service_);
  EXPECT_TRUE(recorder.GetEntries("TtcSystemSpeakerVolume").empty());

  SetVolume(kAudible);
  EXPECT_EQ(recorder.GetEntries("TtcSystemSpeakerVolume").size(), 1u);
}

TEST_F(TtcSessionControllerImplSpeakerVolumeTest,
       UnsupportedPlatformHasNoMonitor) {
  platform_supported_ = false;
  SessionControllerImpl controller(*service_);
  EXPECT_EQ(monitors_created_, 1);
  EXPECT_FALSE(monitor_);
}

}  // namespace ttc
