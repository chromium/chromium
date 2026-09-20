// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/conversation_impl.h"

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/app/test_utils.h"
#include "chrome/browser/ttc/app/ttc_backend.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class MockConversationObserver : public Conversation::Observer {
 public:
  MOCK_METHOD(void,
              OnConversationStateChanged,
              (bool connected,
               const std::string& session_id,
               const std::string& error_message),
              (override));
  MOCK_METHOD(void,
              OnTranscriptions,
              (const std::string& input_transcription,
               const std::string& output_transcription),
              (override));
  MOCK_METHOD(void,
              OnGenerationStateChanged,
              (bool started, bool completed, bool interrupted),
              (override));
};

class FakeSessionController : public SessionController {
 public:
  explicit FakeSessionController(Profile* profile) : profile_(profile) {}
  ~FakeSessionController() override = default;

  // SessionController overrides:
  void GetPageContext(FetchCompleteCallback callback) override {}
  Profile* GetProfile() override { return profile_; }
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
  void UserAudioLevelUpdate(float audio_level) override {}
  void OnSessionInitialized() override {}

  const ToolRequest& last_request() const { return last_request_; }

  void AddToolDefinition(const std::string& name) {
    ToolDefinition tool;
    tool.name = name;
    tools_.push_back(std::move(tool));
  }

 private:
  raw_ptr<Profile> profile_;
  ToolRequest last_request_;
  std::vector<ToolDefinition> tools_;
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
        std::make_unique<AudioController>(GetAudioStreamFactoryBinder(),
                                          GetAudioSystemFactory()),
        session_controller_);
    return *conversation_;
  }

  MockTtcBackend& backend() {
    return static_cast<MockTtcBackend&>(*conversation_->backend());
  }

  AudioController& audio_controller() {
    return *conversation_->audio_controller();
  }

  // Returns a binder dropping the audio stream factory receiver, so that no
  // real audio service is reached.
  AudioController::AudioStreamFactoryBinder GetAudioStreamFactoryBinder() {
    return base::BindLambdaForTesting(
        [](mojo::PendingReceiver<media::mojom::AudioStreamFactory>) {});
  }

  // Returns a factory handing out AudioSystems backed by `audio_manager_`.
  AudioController::AudioSystemFactory GetAudioSystemFactory() {
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
  EXPECT_FALSE(conversation.is_connected());
  EXPECT_NE(conversation.audio_controller(), nullptr);
  EXPECT_NE(conversation.backend(), nullptr);
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

TEST_F(ConversationImplTest, ObserverReceivesTranscriptionsAndState) {
  ConversationImpl& conversation = CreateConversation();

  MockConversationObserver observer;
  conversation.AddObserver(&observer);

  EXPECT_CALL(observer, OnTranscriptions("hello", "world")).Times(1);
  conversation.OnTranscriptions("hello", "world");

  EXPECT_CALL(observer, OnConversationStateChanged(true, "sess_123", ""))
      .Times(1);
  conversation.OnStreamingStateChanged(true, "sess_123", "");

  conversation.RemoveObserver(&observer);
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
  std::vector<int16_t> samples = {4096, 8192, 16384};

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

TEST_F(ConversationImplTest, ConnectionSendsToolSetUpdate) {
  session_controller_.AddToolDefinition("navigate");

  ConversationImpl& conversation = CreateConversation();

  std::vector<std::string> sent_tool_names;
  EXPECT_CALL(backend(), SendToolSetUpdate(testing::_))
      .WillOnce([&sent_tool_names](const std::vector<ToolDefinition>& tools) {
        for (const ToolDefinition& tool : tools) {
          sent_tool_names.push_back(tool.name);
        }
      });
  conversation.OnStreamingStateChanged(/*connected=*/true, "sess_123", "");
  EXPECT_THAT(sent_tool_names, testing::ElementsAre("navigate"));
}

TEST_F(ConversationImplTest, ConnectionWithoutSessionIdSkipsToolSetUpdate) {
  session_controller_.AddToolDefinition("navigate");

  ConversationImpl& conversation = CreateConversation();

  // The backend reports the transport as connected before the server session
  // is set up, at which point there is no session to send the tool set for.
  EXPECT_CALL(backend(), SendToolSetUpdate(testing::_)).Times(0);
  conversation.OnStreamingStateChanged(/*connected=*/true, "", "");
}

TEST_F(ConversationImplTest, DisconnectionDoesNotSendToolSetUpdate) {
  session_controller_.AddToolDefinition("navigate");

  ConversationImpl& conversation = CreateConversation();

  EXPECT_CALL(backend(), SendToolSetUpdate(testing::_)).Times(0);
  conversation.OnStreamingStateChanged(/*connected=*/false, "sess_123",
                                       "some error");
}

}  // namespace ttc
