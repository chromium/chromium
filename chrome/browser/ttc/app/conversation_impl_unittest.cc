// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/conversation_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/ttc/app/audio_controller_impl.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ttc/core/states.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ttc/app/public/error_codes.h"
#include "components/ttc/app/public/session_journal.h"
#include "components/ttc/app/test_utils.h"
#include "components/ttc/app/ttc_backend.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class FakeSessionJournal : public SessionJournal {
 public:
  void Log(std::string_view event_name,
           std::vector<actor::mojom::JournalDetailsPtr> details,
           Track track) override {}
  void HandleServerJournalEvent(const ServerJournalEvent& event) override {}
};

class FakeSessionController : public SessionController {
 public:
  explicit FakeSessionController(Profile* profile) : profile_(profile) {}
  ~FakeSessionController() override = default;

  // SessionController overrides:
  Profile* GetProfile() override { return profile_; }
  SessionJournal& GetJournal() override { return session_journal_; }
  DebugSink* GetDebugSink() override { return debug_sink_; }
  SessionLifecycle GetSessionLifecycle() const override {
    return session_lifecycle_;
  }
  void SetSessionLifecycle(SessionLifecycle lifecycle) override {
    session_lifecycle_ = lifecycle;
  }
  void ProcessToolCall(const ToolRequest& tool_request,
                       ToolResponseCallback tool_response) override {
    last_request_.name = tool_request.name;
    last_request_.arguments = tool_request.arguments.Clone();
    std::move(tool_response).Run(ToolResponse::Success());
  }
  std::vector<ToolDefinition> GetToolDefinitions() override {
    std::vector<ToolDefinition> tools;
    for (const ToolDefinition& tool : tools_) {
      tools.push_back(tool.Clone());
    }
    return tools;
  }
  void UserAudioLevelUpdate(float audio_level) override {
    last_audio_level_ = audio_level;
  }
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  void SendTextInput(const std::string& text) override {}
  void OnModelTurnStarted() override { ++turn_started_count_; }
  void OnModelTurnComplete() override { ++turn_complete_count_; }
  void OnModelInterrupted() override { ++interrupted_count_; }
  void OnInputTranscription(const std::string& text) override {
    last_input_transcription_ = text;
  }
  void OnOutputTranscription(const std::string& text) override {
    last_output_transcription_ = text;
  }
  void OnModelAudioEnvelopeReady(const AudioEnvelope& envelope) override {}
  void OnSessionInitialized() override { ++session_initialized_count_; }
  void OnError(ErrorCode error) override {
    last_error_ = error;
    ++error_count_;
  }
  void EndSessionAsync() override { ++end_session_count_; }

  const ToolRequest& last_request() const { return last_request_; }
  std::optional<float> last_audio_level() const { return last_audio_level_; }
  std::optional<ErrorCode> last_error() const { return last_error_; }
  int error_count() const { return error_count_; }
  int end_session_count() const { return end_session_count_; }
  int session_initialized_count() const { return session_initialized_count_; }
  int turn_started_count() const { return turn_started_count_; }
  int turn_complete_count() const { return turn_complete_count_; }
  int interrupted_count() const { return interrupted_count_; }
  const std::string& last_input_transcription() const {
    return last_input_transcription_;
  }
  const std::string& last_output_transcription() const {
    return last_output_transcription_;
  }

  void AddToolDefinition(const std::string& name) {
    ToolDefinition tool;
    tool.name = name;
    tools_.push_back(std::move(tool));
  }

 private:
  raw_ptr<Profile> profile_;
  ToolRequest last_request_;
  std::optional<float> last_audio_level_;
  std::optional<ErrorCode> last_error_;
  int error_count_ = 0;
  int end_session_count_ = 0;
  int session_initialized_count_ = 0;
  int turn_started_count_ = 0;
  int turn_complete_count_ = 0;
  int interrupted_count_ = 0;
  std::string last_input_transcription_;
  std::string last_output_transcription_;
  std::vector<ToolDefinition> tools_;
  SessionLifecycle session_lifecycle_ = SessionLifecycle::kInitializing;
  FakeSessionJournal session_journal_;
  // Null, as in production while the debug tools are off.
  raw_ptr<DebugSink> debug_sink_ = nullptr;
};

}  // namespace

class ConversationImplTest : public testing::Test {
 public:
  ConversationImplTest() {
    audio_manager_.SetHasInputDevices(true);
    audio_manager_.SetInputStreamParameters(
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Mono(),
                               /*sample_rate=*/48000,
                               /*frames_per_buffer=*/480));
  }

  ~ConversationImplTest() override { audio_manager_.Shutdown(); }

  void TearDown() override {
    // The AudioController must be destroyed before `audio_manager_` is shut
    // down in the destructor.
    conversation_.reset();
  }

 protected:
  // Creates the ConversationImpl under test, wired to a MockTtcBackend and an
  // AudioController backed by `audio_manager_`. Both are reachable via
  // backend() and audio_controller().
  ConversationImpl& CreateConversation() {
    conversation_ = std::make_unique<ConversationImpl>(
        std::make_unique<MockTtcBackend>(),
        std::make_unique<AudioControllerImpl>(GetAudioStreamFactoryBinder(),
                                              GetAudioSystemFactory()),
        session_controller_);
    return *conversation_;
  }

  MockTtcBackend& backend() {
    return static_cast<MockTtcBackend&>(*conversation_->backend());
  }

  AudioControllerImpl& audio_controller() {
    return static_cast<AudioControllerImpl&>(
        *conversation_->audio_controller());
  }

  // Returns a binder dropping the audio stream factory receiver, so that no
  // real audio service is reached.
  AudioControllerImpl::AudioStreamFactoryBinder GetAudioStreamFactoryBinder() {
    return base::BindLambdaForTesting(
        [](mojo::PendingReceiver<media::mojom::AudioStreamFactory>) {});
  }

  // Returns a factory handing out AudioSystems backed by `audio_manager_`.
  AudioControllerImpl::AudioSystemFactory GetAudioSystemFactory() {
    return base::BindLambdaForTesting(
        [this]() -> std::unique_ptr<media::AudioSystem> {
          return std::make_unique<media::AudioSystemImpl>(&audio_manager_);
        });
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeSessionController session_controller_{&profile_};
  media::MockAudioManager audio_manager_{
      std::make_unique<media::TestAudioThread>()};
  std::unique_ptr<ConversationImpl> conversation_;
};

TEST_F(ConversationImplTest, ConstructorInitializesComponents) {
  ConversationImpl& conversation = CreateConversation();
  EXPECT_NE(conversation.audio_controller(), nullptr);
  ASSERT_NE(conversation.backend(), nullptr);
  EXPECT_FALSE(conversation.backend()->is_transport_connected());
}

TEST_F(ConversationImplTest, AudioOutputPlaysToAudioController) {
  ConversationImpl& conversation = CreateConversation();

  std::vector<int16_t> audio_data(1600, 0x1515);
  EXPECT_FALSE(audio_controller().is_playing());

  conversation.OnAudioOutput(audio_data, /*sequence_number=*/1);
  EXPECT_TRUE(audio_controller().is_playing());

  auto bus = media::AudioBus::Create(1, 1600);
  int frames = audio_controller().Render(base::TimeDelta(),
                                         base::TimeTicks::Now(), {}, bus.get());
  EXPECT_EQ(frames, 1600);
}

TEST_F(ConversationImplTest, InterruptionClearsAudioQueue) {
  ConversationImpl& conversation = CreateConversation();

  std::vector<int16_t> audio_data(1600, 0x2525);
  conversation.OnAudioOutput(audio_data, /*sequence_number=*/2);
  EXPECT_TRUE(audio_controller().is_playing());

  // Interruption triggers queue flush
  conversation.OnGenerationStateChanged(/*started=*/false, /*completed=*/false,
                                        /*interrupted=*/true);
  EXPECT_FALSE(audio_controller().is_playing());
}

TEST_F(ConversationImplTest, BackendErrorIsReportedToSessionController) {
  ConversationImpl& conversation = CreateConversation();

  EXPECT_EQ(session_controller_.last_error(), std::nullopt);

  conversation.OnBackendError(ErrorCode::kRateLimited);
  EXPECT_EQ(session_controller_.last_error(), ErrorCode::kRateLimited);
}

TEST_F(ConversationImplTest, BackendClosedEndsSession) {
  ConversationImpl& conversation = CreateConversation();

  conversation.OnBackendInitialized();
  EXPECT_EQ(session_controller_.GetSessionLifecycle(), SessionLifecycle::kLive);

  conversation.OnBackendClosed();
  EXPECT_EQ(session_controller_.end_session_count(), 1);
  EXPECT_EQ(session_controller_.error_count(), 0);
}

TEST_F(ConversationImplTest, ToolCallForwardedToSessionController) {
  ConversationImpl& conversation = CreateConversation();

  ToolRequest tool_request;
  tool_request.name = "navigate";
  tool_request.arguments.Set("url", "abc");

  std::optional<ToolResponse> response;
  conversation.OnToolCall(std::move(tool_request),
                          base::BindLambdaForTesting([&](ToolResponse result) {
                            response = std::move(result);
                          }));

  // The request should have reached the SessionController (unchanged).
  EXPECT_EQ(session_controller_.last_request().name, "navigate");
  const std::string* url =
      session_controller_.last_request().arguments.FindString("url");
  ASSERT_TRUE(url);
  EXPECT_EQ(*url, "abc");

  // The response should be routed back from the SessionController.
  ASSERT_TRUE(response.has_value());
  EXPECT_TRUE(response->Ok());
}

TEST_F(ConversationImplTest, StartAndStopWiring) {
  ConversationImpl& conversation = CreateConversation();

  EXPECT_CALL(backend(), Connect(&conversation)).Times(1);
  conversation.Start();
  EXPECT_FALSE(audio_controller().is_capturing());

  conversation.OnBackendInitialized();
  EXPECT_TRUE(audio_controller().is_capturing());

  EXPECT_CALL(backend(), Close()).Times(1);
  conversation.Stop();
  EXPECT_FALSE(audio_controller().is_capturing());
}

TEST_F(ConversationImplTest, CapturedAudioRoutedToBackend) {
  ConversationImpl& conversation = CreateConversation();
  conversation.Start();

  // Use values that have exact representations in float:
  // e.g. 4096 = 4096/32768 = 0.125f, 8192 = 0.25f, 16384 = 0.5f.
  // The FIFO expects a full 100ms chunk (1600 frames at 16kHz) to emit.
  const size_t kNumFrames = 1600;
  std::vector<int16_t> samples(kNumFrames);
  for (size_t i = 0; i < kNumFrames; ++i) {
    samples[i] = (i % 2 == 0) ? 4096 : 8192;
  }

  base::RunLoop run_loop;
  EXPECT_CALL(backend(), SendAudioChunk(testing::ElementsAreArray(samples)))
      .WillOnce([&run_loop] { run_loop.Quit(); });

  auto bus = media::AudioBus::Create(1, samples.size());
  for (size_t i = 0; i < samples.size(); ++i) {
    bus->channel(0)[i] =
        media::SignedInt16SampleTypeTraits::ToFloat(samples[i]);
  }
  audio_controller().Capture(bus.get(), base::TimeTicks::Now(), {}, 1.0);
  run_loop.Run();
}

TEST_F(ConversationImplTest, CapturedAudioLevelReportedToSession) {
  ConversationImpl& conversation = CreateConversation();
  conversation.Start();

  base::RunLoop run_loop;
  EXPECT_CALL(backend(), SendAudioChunk(testing::_)).WillOnce([&run_loop] {
    run_loop.Quit();
  });

  auto bus = media::AudioBus::Create(1, 1600);
  constexpr float kAmplitude = 0.25f;
  std::ranges::fill(bus->channel(0), kAmplitude);
  audio_controller().Capture(bus.get(), base::TimeTicks::Now(), {}, 1.0);
  run_loop.Run();

  ASSERT_TRUE(session_controller_.last_audio_level().has_value());
  EXPECT_FLOAT_EQ(*session_controller_.last_audio_level(), kAmplitude);
}

TEST_F(ConversationImplTest, OnPlaybackCompletedReportsStatus) {
  ConversationImpl& conversation = CreateConversation();
  conversation.Start();

  base::RunLoop run_loop;
  EXPECT_CALL(backend(), ReportPlaybackStatus(42)).WillOnce([&run_loop] {
    run_loop.Quit();
  });

  std::vector<int16_t> samples(100, 1000);
  audio_controller().PlayAudio(samples, /*sequence_number=*/42);

  auto bus = media::AudioBus::Create(1, 100);
  audio_controller().Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                            bus.get());
  run_loop.Run();
}

TEST_F(ConversationImplTest, SendTextInputForwardsToBackend) {
  ConversationImpl& conversation = CreateConversation();

  EXPECT_CALL(backend(), SendTextInput("hello world")).Times(1);
  conversation.SendTextInput("hello world");
}

TEST_F(ConversationImplTest, BackendInitializedSendsToolSetUpdate) {
  session_controller_.AddToolDefinition("navigate");

  ConversationImpl& conversation = CreateConversation();

  std::vector<std::string> sent_tool_names;
  EXPECT_CALL(backend(), SendToolSetUpdate(testing::_))
      .WillOnce([&sent_tool_names](const std::vector<ToolDefinition>& tools) {
        for (const ToolDefinition& tool : tools) {
          sent_tool_names.push_back(tool.name);
        }
      });
  conversation.OnBackendInitialized();
  EXPECT_THAT(sent_tool_names, testing::ElementsAre("navigate"));
}

TEST_F(ConversationImplTest, BackendShutdownDoesNotSendToolSetUpdate) {
  session_controller_.AddToolDefinition("navigate");

  ConversationImpl& conversation = CreateConversation();

  EXPECT_CALL(backend(), SendToolSetUpdate(testing::_)).Times(0);
  conversation.OnBackendClosed();
}

TEST_F(ConversationImplTest, BackendErrorDoesNotSendToolSetUpdate) {
  session_controller_.AddToolDefinition("navigate");

  ConversationImpl& conversation = CreateConversation();

  // The tool set is only sent once the backend is initialized; a session
  // that closes or fails has nothing to send it for.
  EXPECT_CALL(backend(), SendToolSetUpdate(testing::_)).Times(0);
  conversation.OnBackendError(ErrorCode::kUnknown);
}

TEST_F(ConversationImplTest, AudioCaptureErrorIsReported) {
  audio_manager_.SetHasInputDevices(false);
  ConversationImpl& conversation = CreateConversation();

  conversation.Start();
  conversation.OnBackendInitialized();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return session_controller_.error_count() > 0; }));

  EXPECT_EQ(session_controller_.error_count(), 1);
  EXPECT_EQ(session_controller_.last_error(),
            ErrorCode::kAudioNoMicrophoneDetected);
  EXPECT_FALSE(audio_controller().is_capturing());
}

TEST_F(ConversationImplTest, AudioCaptureErrorAfterStopIsIgnored) {
  ConversationImpl& conversation = CreateConversation();
  conversation.Start();
  conversation.Stop();

  audio_controller().OnCaptureError(
      media::AudioCapturerSource::ErrorCode::kDeviceRemoved, "error");

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(session_controller_.error_count(), 0);
}

TEST_F(ConversationImplTest,
       SessionControllerNotifiedOfTurnAndTranscriptionEvents) {
  ConversationImpl& conversation = CreateConversation();

  conversation.OnTranscriptions("user said hello", "agent said hi");
  EXPECT_EQ(session_controller_.last_input_transcription(), "user said hello");
  EXPECT_EQ(session_controller_.last_output_transcription(), "agent said hi");

  conversation.OnGenerationStateChanged(/*started=*/true, /*completed=*/false,
                                        /*interrupted=*/false);
  EXPECT_EQ(session_controller_.turn_started_count(), 1);

  conversation.OnGenerationStateChanged(/*started=*/false, /*completed=*/false,
                                        /*interrupted=*/true);
  EXPECT_EQ(session_controller_.interrupted_count(), 1);

  conversation.OnGenerationStateChanged(/*started=*/false, /*completed=*/true,
                                        /*interrupted=*/false);
  EXPECT_EQ(session_controller_.turn_complete_count(), 1);
}

TEST_F(ConversationImplTest, SessionGoesLiveOnlyOnceBackendInitialized) {
  ConversationImpl& conversation = CreateConversation();

  EXPECT_CALL(backend(), Connect(&conversation)).WillOnce([&] {
    EXPECT_EQ(session_controller_.GetSessionLifecycle(),
              SessionLifecycle::kInitializing);
  });
  conversation.Start();
  EXPECT_EQ(session_controller_.GetSessionLifecycle(),
            SessionLifecycle::kInitializing);
  EXPECT_EQ(session_controller_.session_initialized_count(), 0);

  conversation.OnBackendInitialized();
  EXPECT_EQ(session_controller_.GetSessionLifecycle(), SessionLifecycle::kLive);
  EXPECT_EQ(session_controller_.session_initialized_count(), 1);
}

TEST_F(ConversationImplTest, MuteStopsAndResumesCapture) {
  ConversationImpl& conversation = CreateConversation();
  conversation.Start();
  conversation.OnBackendInitialized();
  EXPECT_TRUE(audio_controller().is_capturing());
  EXPECT_FALSE(conversation.IsMicMuted());

  conversation.SetMicMuted(true);
  EXPECT_TRUE(conversation.IsMicMuted());
  EXPECT_FALSE(audio_controller().is_capturing());

  conversation.SetMicMuted(false);
  EXPECT_FALSE(conversation.IsMicMuted());
  EXPECT_TRUE(audio_controller().is_capturing());
}

TEST_F(ConversationImplTest, AudioCapturedBeforeMuteIsNotSent) {
  ConversationImpl& conversation = CreateConversation();
  conversation.Start();
  conversation.OnBackendInitialized();

  // A full chunk is captured and posted to the main thread...
  auto bus = media::AudioBus::Create(1, 1600);
  std::ranges::fill(bus->channel(0), 0.25f);
  audio_controller().Capture(bus.get(), base::TimeTicks::Now(), {}, 1.0);

  // ...but the user mutes before it arrives.
  EXPECT_CALL(backend(), SendAudioChunk(testing::_)).Times(0);
  conversation.SetMicMuted(true);
  // Runs the tasks posted so far, including the one delivering the chunk.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  ASSERT_TRUE(session_controller_.last_audio_level().has_value());
  EXPECT_FLOAT_EQ(*session_controller_.last_audio_level(), 0.0f);
}

}  // namespace ttc
