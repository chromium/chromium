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
#include "chrome/browser/ttc/app/ttc_backend.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class MockTtcBackend : public TtcBackend {
 public:
  MockTtcBackend() {
    ON_CALL(*this, Connect()).WillByDefault([this]() { is_connected_ = true; });
    ON_CALL(*this, Close()).WillByDefault([this]() { is_connected_ = false; });
    ON_CALL(*this, is_connected()).WillByDefault([this]() {
      return is_connected_;
    });
  }
  ~MockTtcBackend() override = default;

  void set_observer(TtcBackend::Observer* observer) override {
    observer_ = observer;
  }
  TtcBackend::Observer* observer() const { return observer_; }

  MOCK_METHOD(void, Connect, (), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(bool, is_connected, (), (const, override));
  MOCK_METHOD(void, SendAudioChunk, (const std::vector<uint8_t>&), (override));
  MOCK_METHOD(void, SendTextInput, (const std::string&), (override));
  MOCK_METHOD(void,
              SendContextUpdate,
              (const GURL&,
               const std::string&,
               const optimization_guide::proto::AnnotatedPageContent&),
              (override));
  MOCK_METHOD(void, ReportPlaybackStatus, (int64_t), (override));
  MOCK_METHOD(void,
              SendToolSetUpdate,
              (const std::vector<ToolDefinition>&),
              (override));

 private:
  bool is_connected_ = false;
  raw_ptr<TtcBackend::Observer> observer_ = nullptr;
};

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
  MOCK_METHOD(void,
              OnToolCall,
              (const std::string& name,
               base::DictValue arguments,
               Conversation::Observer::ToolResponseCallback response_callback),
              (override));
};

}  // namespace

class ConversationImplTest : public testing::Test {
 public:
  ConversationImplTest() = default;
  ~ConversationImplTest() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ConversationImplTest, DefaultConstructorInitializesComponents) {
  ConversationImpl conversation(&profile_);
  EXPECT_FALSE(conversation.is_connected());
  EXPECT_NE(conversation.audio_controller(), nullptr);
  EXPECT_NE(conversation.backend(), nullptr);
}

TEST_F(ConversationImplTest, AudioOutputPlaysToAudioController) {
  auto audio_controller = std::make_unique<AudioController>();
  AudioController* audio_controller_ptr = audio_controller.get();
  auto mock_backend = std::make_unique<MockTtcBackend>();

  ConversationImpl conversation(std::move(mock_backend),
                                std::move(audio_controller));

  std::vector<uint8_t> audio_data(3200, 0x15);
  EXPECT_FALSE(audio_controller_ptr->is_playing());

  conversation.OnAudioOutput(audio_data, /*sequence_number=*/1);
  EXPECT_TRUE(audio_controller_ptr->is_playing());

  auto bus = media::AudioBus::Create(1, 1600);
  int frames = audio_controller_ptr->Render(base::TimeDelta(),
                                            base::TimeTicks::Now(), {},
                                            bus.get());
  EXPECT_EQ(frames, 1600);
}

TEST_F(ConversationImplTest, InterruptionClearsAudioQueue) {
  auto audio_controller = std::make_unique<AudioController>();
  AudioController* audio_controller_ptr = audio_controller.get();
  auto mock_backend = std::make_unique<MockTtcBackend>();

  ConversationImpl conversation(std::move(mock_backend),
                                std::move(audio_controller));

  std::vector<uint8_t> audio_data(3200, 0x25);
  conversation.OnAudioOutput(audio_data, /*sequence_number=*/2);
  EXPECT_TRUE(audio_controller_ptr->is_playing());

  // Interruption triggers queue flush
  conversation.OnGenerationStateChanged(/*started=*/false, /*completed=*/false,
                                        /*interrupted=*/true);
  EXPECT_FALSE(audio_controller_ptr->is_playing());
}

TEST_F(ConversationImplTest, ObserverReceivesTranscriptionsAndTools) {
  auto audio_controller = std::make_unique<AudioController>();
  auto mock_backend = std::make_unique<MockTtcBackend>();

  ConversationImpl conversation(std::move(mock_backend),
                                std::move(audio_controller));

  MockConversationObserver observer;
  conversation.AddObserver(&observer);

  EXPECT_CALL(observer, OnTranscriptions("hello", "world")).Times(1);
  conversation.OnTranscriptions("hello", "world");

  bool tool_called = false;
  EXPECT_CALL(observer,
              OnToolCall(testing::Eq("navigate"), testing::_, testing::_))
      .WillOnce([&](const std::string& name, base::DictValue args,
                    Conversation::Observer::ToolResponseCallback cb) {
        tool_called = true;
        EXPECT_EQ(name, "navigate");
        const std::string* url = args.FindString("url");
        ASSERT_TRUE(url);
        EXPECT_EQ(*url, "abc");
        std::move(cb).Run(base::DictValue());
      });
  base::DictValue args;
  args.Set("url", "abc");
  conversation.OnToolCall("navigate", std::move(args), base::DoNothing());
  EXPECT_TRUE(tool_called);

  EXPECT_CALL(observer, OnConversationStateChanged(true, "sess_123", ""))
      .Times(1);
  conversation.OnStreamingStateChanged(true, "sess_123", "");

  conversation.RemoveObserver(&observer);
}

TEST_F(ConversationImplTest, Downsample48kHzTo16kHz) {
  // Test basic downsampling: 6 samples downsampled to 2.
  // Group 1: 300, 300, 300 -> 300
  // Group 2: -300, -300, -300 -> -300
  std::vector<int16_t> samples_48k = {300, 300, 300, -300, -300, -300};
  auto input_span = base::as_byte_span(samples_48k);
  std::vector<uint8_t> downsampled = Downsample48kHzTo16kHz(input_span);

  ASSERT_EQ(downsampled.size(), 2 * sizeof(int16_t));
  int16_t out[2] = {};
  base::as_writable_byte_span(out).copy_from(downsampled);
  EXPECT_EQ(out[0], 300);
  EXPECT_EQ(out[1], -300);
}

TEST_F(ConversationImplTest, Downsample48kHzTo16kHzEdgeCases) {
  // Empty span
  EXPECT_TRUE(Downsample48kHzTo16kHz(base::span<const uint8_t>()).empty());

  // Less than 3 samples (< 6 bytes)
  std::vector<int16_t> one_sample = {100};
  EXPECT_TRUE(Downsample48kHzTo16kHz(base::as_byte_span(one_sample)).empty());

  // Odd number of bytes (unaligned)
  std::vector<uint8_t> unaligned_bytes = {1, 2, 3, 4, 5};
  EXPECT_TRUE(Downsample48kHzTo16kHz(unaligned_bytes).empty());

  // Remainder samples not divisible by 3 (e.g. 7 samples -> 2 downsampled
  // output samples)
  std::vector<int16_t> seven_samples = {300, 300, 300, -300, -300, -300, 1000};
  std::vector<uint8_t> downsampled_seven =
      Downsample48kHzTo16kHz(base::as_byte_span(seven_samples));
  ASSERT_EQ(downsampled_seven.size(), 2 * sizeof(int16_t));
  int16_t out_seven[2] = {};
  base::as_writable_byte_span(out_seven).copy_from(downsampled_seven);
  EXPECT_EQ(out_seven[0], 300);
  EXPECT_EQ(out_seven[1], -300);
}

TEST_F(ConversationImplTest, StartAndStopWiring) {
  auto fake_binder = base::BindLambdaForTesting(
      [](mojo::PendingReceiver<media::mojom::AudioStreamFactory>) {});
  auto audio_controller = std::make_unique<AudioController>(fake_binder);
  AudioController* audio_controller_ptr = audio_controller.get();

  auto mock_backend = std::make_unique<MockTtcBackend>();
  MockTtcBackend* backend_ptr = mock_backend.get();

  ConversationImpl conversation(std::move(mock_backend),
                                std::move(audio_controller));

  EXPECT_CALL(*backend_ptr, Connect()).Times(1);
  conversation.Start();
  EXPECT_TRUE(audio_controller_ptr->is_capturing());

  EXPECT_CALL(*backend_ptr, Close()).Times(1);
  conversation.Stop();
  EXPECT_FALSE(audio_controller_ptr->is_capturing());
}

TEST_F(ConversationImplTest, CapturedAudioRouting16kHzPassthrough) {
  auto audio_controller = std::make_unique<AudioController>();
  AudioController* audio_controller_ptr = audio_controller.get();
  auto mock_backend = std::make_unique<MockTtcBackend>();
  MockTtcBackend* backend_ptr = mock_backend.get();

  ConversationImpl conversation(std::move(mock_backend),
                                std::move(audio_controller));
  conversation.Start();

  // Use values that have exact representations in float:
  // e.g. 4096 = 4096/32768 = 0.125f, 8192 = 0.25f, 16384 = 0.5f.
  std::vector<int16_t> samples = {4096, 8192, 16384};
  auto sample_bytes = base::as_byte_span(samples);
  std::vector<uint8_t> expected_bytes(sample_bytes.begin(), sample_bytes.end());

  base::RunLoop run_loop;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  // On Mac and Android, AudioController::Capture produces 48kHz audio.
  // 3 samples at 48kHz downsamples to 1 sample: (4096+8192+16384)/3 = 9557.
  std::vector<int16_t> downsampled_sample = {9557};
  auto downsampled_bytes = base::as_byte_span(downsampled_sample);
  std::vector<uint8_t> expected_downsampled(downsampled_bytes.begin(),
                                            downsampled_bytes.end());
  EXPECT_CALL(*backend_ptr, SendAudioChunk(expected_downsampled))
      .WillOnce([&run_loop] { run_loop.Quit(); });
#else
  EXPECT_CALL(*backend_ptr, SendAudioChunk(expected_bytes))
      .WillOnce([&run_loop] { run_loop.Quit(); });
#endif

  auto bus = media::AudioBus::Create(1, samples.size());
  for (size_t i = 0; i < samples.size(); ++i) {
    bus->channel(0)[i] =
        media::SignedInt16SampleTypeTraits::ToFloat(samples[i]);
  }
  audio_controller_ptr->Capture(bus.get(), base::TimeTicks::Now(), {}, 1.0);
  run_loop.Run();
}

TEST_F(ConversationImplTest, CapturedAudioRouting48kHzDownsampling) {
  auto audio_controller = std::make_unique<AudioController>();
  AudioController* audio_controller_ptr = audio_controller.get();
  auto mock_backend = std::make_unique<MockTtcBackend>();
  MockTtcBackend* backend_ptr = mock_backend.get();

  ConversationImpl conversation(std::move(mock_backend),
                                std::move(audio_controller));
  conversation.Start();

  // 6 samples with exact float representations
  std::vector<int16_t> samples = {1024, 2048, 4096, -1024, -2048, -4096};
  // (1024+2048+4096)/3 = 2389, (-1024-2048-4096)/3 = -2389
  std::vector<int16_t> expected_downsampled = {2389, -2389};
  auto expected_bytes_span = base::as_byte_span(expected_downsampled);
  std::vector<uint8_t> expected_bytes(expected_bytes_span.begin(),
                                      expected_bytes_span.end());

  base::RunLoop run_loop;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(*backend_ptr, SendAudioChunk(expected_bytes))
      .WillOnce([&run_loop] { run_loop.Quit(); });
#else
  // On platforms where capture is 16kHz, passthrough occurs
  auto sample_bytes = base::as_byte_span(samples);
  std::vector<uint8_t> raw_bytes(sample_bytes.begin(), sample_bytes.end());
  EXPECT_CALL(*backend_ptr, SendAudioChunk(raw_bytes)).WillOnce([&run_loop] {
    run_loop.Quit();
  });
#endif

  auto bus = media::AudioBus::Create(1, samples.size());
  for (size_t i = 0; i < samples.size(); ++i) {
    bus->channel(0)[i] =
        media::SignedInt16SampleTypeTraits::ToFloat(samples[i]);
  }
  audio_controller_ptr->Capture(bus.get(), base::TimeTicks::Now(), {}, 1.0);
  run_loop.Run();
}

TEST_F(ConversationImplTest, OnPlaybackCompletedReportsStatus) {
  auto audio_controller = std::make_unique<AudioController>();
  AudioController* audio_controller_ptr = audio_controller.get();
  auto mock_backend = std::make_unique<MockTtcBackend>();
  MockTtcBackend* backend_ptr = mock_backend.get();

  ConversationImpl conversation(std::move(mock_backend),
                                std::move(audio_controller));
  conversation.Start();

  base::RunLoop run_loop;
  EXPECT_CALL(*backend_ptr, ReportPlaybackStatus(42)).WillOnce([&run_loop] {
    run_loop.Quit();
  });

  std::vector<int16_t> samples(100, 1000);
  audio_controller_ptr->PlayAudio(base::as_byte_span(samples),
                                  /*sequence_number=*/42);

  auto bus = media::AudioBus::Create(1, 100);
  audio_controller_ptr->Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                               bus.get());
  run_loop.Run();
}

TEST_F(ConversationImplTest, SendTextInputForwardsToBackend) {
  auto mock_backend = std::make_unique<MockTtcBackend>();
  MockTtcBackend* backend_ptr = mock_backend.get();

  ConversationImpl conversation(std::move(mock_backend), nullptr);
  EXPECT_CALL(*backend_ptr, SendTextInput("hello world")).Times(1);
  conversation.SendTextInput("hello world");
}

TEST_F(ConversationImplTest, SendToolSetUpdateForwardsToBackend) {
  auto mock_backend = std::make_unique<MockTtcBackend>();
  MockTtcBackend* backend_ptr = mock_backend.get();

  ConversationImpl conversation(std::move(mock_backend), nullptr);
  std::vector<ToolDefinition> tools;
  ToolDefinition tool;
  tool.name = "test_tool";
  tools.push_back(std::move(tool));
  EXPECT_CALL(*backend_ptr, SendToolSetUpdate(testing::_)).Times(1);
  conversation.SendToolSetUpdate(tools);
}

}  // namespace ttc
