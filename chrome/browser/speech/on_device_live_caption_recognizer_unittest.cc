// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/on_device_live_caption_recognizer.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace speech {

namespace {

class FakeAsrStreamInput : public on_device_model::mojom::AsrStreamInput {
 public:
  FakeAsrStreamInput(
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> receiver,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder)
      : receiver_(this, std::move(receiver)),
        responder_(std::move(responder)) {}

  void AddAudioChunk(on_device_model::mojom::AudioDataPtr data) override {
    audio_chunks_.push_back(std::move(data));
    std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> results;
    auto result = on_device_model::mojom::SpeechRecognitionResult::New();
    result->transcript = "OnDeviceModel transcription";
    result->is_final = true;
    results.push_back(std::move(result));
    if (responder_.is_bound()) {
      responder_->OnResponse(std::move(results));
    }
  }

  const std::vector<on_device_model::mojom::AudioDataPtr>& audio_chunks()
      const {
    return audio_chunks_;
  }

  void ClearAudioChunks() { audio_chunks_.clear(); }
  void ResetReceiver() { receiver_.reset(); }

 private:
  mojo::Receiver<on_device_model::mojom::AsrStreamInput> receiver_;
  mojo::Remote<on_device_model::mojom::AsrStreamResponder> responder_;
  std::vector<on_device_model::mojom::AudioDataPtr> audio_chunks_;
};

class FakeSession : public on_device_model::mojom::Session {
 public:
  explicit FakeSession(
      mojo::PendingReceiver<on_device_model::mojom::Session> receiver)
      : receiver_(this, std::move(receiver)) {}

  void Append(on_device_model::mojom::AppendOptionsPtr options,
              mojo::PendingRemote<on_device_model::mojom::ContextClient> client)
      override {}
  void Generate(on_device_model::mojom::GenerateOptionsPtr options,
                mojo::PendingRemote<on_device_model::mojom::StreamingResponder>
                    responder) override {}
  void GetSizeInTokens(on_device_model::mojom::InputPtr input,
                       GetSizeInTokensCallback callback) override {}
  void Score(const std::string& text, ScoreCallback callback) override {}
  void Clone(
      mojo::PendingReceiver<on_device_model::mojom::Session> session) override {
  }
  void GetProbabilitiesBlocking(
      const std::string& text,
      GetProbabilitiesBlockingCallback callback) override {}
  void SetPriority(on_device_model::mojom::Priority priority) override {}
  void AsrStream(
      on_device_model::mojom::AsrStreamOptionsPtr options,
      mojo::PendingReceiver<on_device_model::mojom::AsrStreamInput> input,
      mojo::PendingRemote<on_device_model::mojom::AsrStreamResponder> responder)
      override {}
  void Hint(on_device_model::mojom::HintOptionsPtr options) override {}

  bool is_bound() const { return receiver_.is_bound(); }

  void set_disconnect_handler(base::OnceClosure handler) {
    receiver_.set_disconnect_handler(std::move(handler));
  }

 private:
  mojo::Receiver<on_device_model::mojom::Session> receiver_;
};

}  // namespace

class OnDeviceLiveCaptionRecognizerTest
    : public testing::Test,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  OnDeviceLiveCaptionRecognizerTest() = default;
  ~OnDeviceLiveCaptionRecognizerTest() override = default;

  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& event,
      OnSpeechRecognitionRecognitionEventCallback callback) override {
    last_received_result_ = event;
    std::move(callback).Run(recognition_event_success_response_);
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnSpeechRecognitionStopped() override {
    speech_recognition_stopped_called_ = true;
    speech_recognition_stopped_count_++;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  void OnSpeechRecognitionError() override {
    error_occurred_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override {}

  media::mojom::SpeechRecognitionOptionsPtr CreateOptions() {
    auto options = media::mojom::SpeechRecognitionOptions::New();
    options->recognition_mode = media::mojom::SpeechRecognitionMode::kCaption;
    options->enable_formatting = false;
    options->recognizer_client_type =
        media::mojom::RecognizerClientType::kLiveCaption;
    options->skip_continuously_empty_audio = false;
    return options;
  }

 protected:
  void WaitForRecognitionEvent() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  void SendAudio(OnDeviceLiveCaptionRecognizer* recognizer,
                 base::TimeDelta duration,
                 int sample_rate = 16000,
                 int channel_count = 1,
                 int16_t sample_value = 100) {
    auto audio_buffer = media::mojom::AudioDataS16::New();
    audio_buffer->sample_rate = sample_rate;
    audio_buffer->channel_count = channel_count;
    audio_buffer->frame_count = duration.InSecondsF() * sample_rate;
    audio_buffer->data.resize(
        audio_buffer->frame_count * audio_buffer->channel_count, sample_value);
    recognizer->SendAudioToSpeechRecognitionService(std::move(audio_buffer),
                                                    base::Seconds(0));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient> receiver_{
      this};
  std::unique_ptr<base::RunLoop> run_loop_;
  media::SpeechRecognitionResult last_received_result_;
  bool error_occurred_ = false;
  bool speech_recognition_stopped_called_ = false;
  int speech_recognition_stopped_count_ = 0;
  bool recognition_event_success_response_ = true;
};

TEST_F(OnDeviceLiveCaptionRecognizerTest, SendAudioAndReceiveTranscription) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  SendAudio(recognizer.get(), base::Seconds(1));
  WaitForRecognitionEvent();
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, DisconnectAsrStreamTriggersError) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  auto fake_asr_input = std::make_unique<FakeAsrStreamInput>(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  SendAudio(recognizer.get(), base::Seconds(1));
  WaitForRecognitionEvent();
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);

  fake_asr_input.reset();
  ASSERT_TRUE(base::test::RunUntil([&]() { return error_occurred_; }));
  EXPECT_TRUE(error_occurred_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, MarkDoneResetsAsrStreamInput) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  SendAudio(recognizer.get(), base::Seconds(1));
  WaitForRecognitionEvent();
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);

  recognizer->MarkDone();
  SendAudio(recognizer.get(), base::Seconds(1));
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(error_occurred_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, SendAudioWithResampling) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  auto audio_buffer = media::mojom::AudioDataS16::New();
  audio_buffer->sample_rate = 48000;
  audio_buffer->channel_count = 1;
  audio_buffer->frame_count = 48000;
  audio_buffer->data.resize(audio_buffer->frame_count, 100);
  recognizer->SendAudioToSpeechRecognitionService(std::move(audio_buffer),
                                                  base::Seconds(0));
  WaitForRecognitionEvent();
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       SilenceDetectionDropsContinuouslyEmptyAudio) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto options = CreateOptions();
  options->skip_continuously_empty_audio = true;

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), std::move(options),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // 1. Send initial non-empty audio.
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/16000,
            /*channel_count=*/1, /*sample_value=*/100);
  WaitForRecognitionEvent();
  EXPECT_FALSE(fake_asr_input.audio_chunks().empty());

  // 2. Send empty audio within the 10-second threshold (e.g. after 2 seconds).
  fake_asr_input.ClearAudioChunks();
  task_environment_.FastForwardBy(base::Seconds(2));
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/16000,
            /*channel_count=*/1, /*sample_value=*/0);
  WaitForRecognitionEvent();
  EXPECT_FALSE(fake_asr_input.audio_chunks().empty());

  // 3. Fast-forward past the 10-second silence threshold (e.g. 11 seconds).
  fake_asr_input.ClearAudioChunks();
  task_environment_.FastForwardBy(base::Seconds(11));
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/16000,
            /*channel_count=*/1, /*sample_value=*/0);
  task_environment_.RunUntilIdle();
  // Buffer should have been dropped due to continuous silence.
  EXPECT_TRUE(fake_asr_input.audio_chunks().empty());

  // 4. Send non-empty audio again to resume recognition.
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/16000,
            /*channel_count=*/1, /*sample_value=*/100);
  WaitForRecognitionEvent();
  EXPECT_FALSE(fake_asr_input.audio_chunks().empty());
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, DynamicSampleRateSwitching) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // 1. Initial audio at 16000 Hz.
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/16000,
            /*channel_count=*/1, /*sample_value=*/100);
  WaitForRecognitionEvent();
  ASSERT_FALSE(fake_asr_input.audio_chunks().empty());
  EXPECT_EQ(16000, fake_asr_input.audio_chunks().back()->sample_rate);
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);

  // 2. Switch dynamically to 48000 Hz.
  fake_asr_input.ClearAudioChunks();
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/48000,
            /*channel_count=*/1, /*sample_value=*/100);
  WaitForRecognitionEvent();
  ASSERT_FALSE(fake_asr_input.audio_chunks().empty());
  EXPECT_EQ(16000, fake_asr_input.audio_chunks().back()->sample_rate);
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);

  // 3. Switch dynamically to 8000 Hz.
  fake_asr_input.ClearAudioChunks();
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/8000,
            /*channel_count=*/1, /*sample_value=*/100);
  WaitForRecognitionEvent();
  ASSERT_FALSE(fake_asr_input.audio_chunks().empty());
  EXPECT_EQ(16000, fake_asr_input.audio_chunks().back()->sample_rate);
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);

  EXPECT_FALSE(error_occurred_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, RejectMalformedAudioBuffers) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  auto test_bad_buffer = [&](int channel_count, int sample_rate,
                             int frame_count, size_t data_size) {
    mojo::FakeMessageDispatchContext dispatch_context;
    mojo::test::BadMessageObserver bad_message_observer;
    auto bad_buffer = media::mojom::AudioDataS16::New();
    bad_buffer->channel_count = channel_count;
    bad_buffer->sample_rate = sample_rate;
    bad_buffer->frame_count = frame_count;
    bad_buffer->data.resize(data_size, 0);

    recognizer->SendAudioToSpeechRecognitionService(std::move(bad_buffer),
                                                    base::Seconds(0));
    EXPECT_EQ(
        "Invalid audio data received from renderer in Live Caption recognizer.",
        bad_message_observer.WaitForBadMessage());
  };

  // Channel count <= 0
  test_bad_buffer(/*channel_count=*/0, /*sample_rate=*/16000,
                  /*frame_count=*/100, /*data_size=*/0);
  test_bad_buffer(/*channel_count=*/-1, /*sample_rate=*/16000,
                  /*frame_count=*/100, /*data_size=*/100);

  // Channel count > kMaxChannels
  test_bad_buffer(/*channel_count=*/media::limits::kMaxChannels + 1,
                  /*sample_rate=*/16000, /*frame_count=*/100,
                  /*data_size=*/100);

  // Sample rate <= 0
  test_bad_buffer(/*channel_count=*/1, /*sample_rate=*/0,
                  /*frame_count=*/100, /*data_size=*/100);
  test_bad_buffer(/*channel_count=*/1, /*sample_rate=*/-1,
                  /*frame_count=*/100, /*data_size=*/100);

  // Sample rate < kMinSampleRate
  test_bad_buffer(/*channel_count=*/1,
                  /*sample_rate=*/media::limits::kMinSampleRate - 1,
                  /*frame_count=*/100, /*data_size=*/100);

  // Sample rate > kMaxSampleRate
  test_bad_buffer(/*channel_count=*/1,
                  /*sample_rate=*/media::limits::kMaxSampleRate + 1,
                  /*frame_count=*/100, /*data_size=*/100);

  // Frame count <= 0
  test_bad_buffer(/*channel_count=*/1, /*sample_rate=*/16000,
                  /*frame_count=*/0, /*data_size=*/0);
  test_bad_buffer(/*channel_count=*/1, /*sample_rate=*/16000,
                  /*frame_count=*/-1, /*data_size=*/100);

  // Frame count > kMaxSamplesPerPacket
  test_bad_buffer(/*channel_count=*/1, /*sample_rate=*/16000,
                  /*frame_count=*/media::limits::kMaxSamplesPerPacket + 1,
                  /*data_size=*/media::limits::kMaxSamplesPerPacket + 1);

  // Data size smaller than frame_count * channel_count
  test_bad_buffer(/*channel_count=*/1, /*sample_rate=*/16000,
                  /*frame_count=*/100, /*data_size=*/50);

  // Data size larger than frame_count * channel_count
  test_bad_buffer(/*channel_count=*/1, /*sample_rate=*/16000,
                  /*frame_count=*/100, /*data_size=*/200);

  // Null audio buffer
  {
    mojo::FakeMessageDispatchContext dispatch_context;
    mojo::test::BadMessageObserver bad_message_observer;
    recognizer->SendAudioToSpeechRecognitionService(nullptr, base::Seconds(0));
    EXPECT_EQ(
        "Invalid audio data received from renderer in Live Caption recognizer.",
        bad_message_observer.WaitForBadMessage());
  }

  // Overflow in frame_count * channel_count
  test_bad_buffer(/*channel_count=*/2, /*sample_rate=*/16000,
                  /*frame_count=*/std::numeric_limits<int>::max(),
                  /*data_size=*/100);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, SessionLifetimeBoundedByRecognizer) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  mojo::Remote<on_device_model::mojom::Session> session_remote;
  FakeSession fake_session(session_remote.BindNewPipeAndPassReceiver());

  bool session_disconnected = false;
  fake_session.set_disconnect_handler(base::BindOnce(
      [](bool* disconnected) { *disconnected = true; }, &session_disconnected));

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      std::move(session_remote), std::move(asr_stream_input),
      std::move(asr_stream_responder));

  EXPECT_TRUE(fake_session.is_bound());
  EXPECT_FALSE(session_disconnected);

  // Destroying the recognizer should destroy session_ and trigger
  // disconnection.
  recognizer.reset();
  ASSERT_TRUE(base::test::RunUntil([&]() { return session_disconnected; }));
  EXPECT_TRUE(session_disconnected);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, NonStandardChannelLayout3Channels) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // Send 3-channel audio at 48000 Hz, which has an unsupported standard layout
  // and requires discrete channel layout handling and resampling to 16000 Hz
  // mono.
  SendAudio(recognizer.get(), base::Seconds(1), /*sample_rate=*/48000,
            /*channel_count=*/3, /*sample_value=*/100);
  WaitForRecognitionEvent();
  ASSERT_FALSE(fake_asr_input.audio_chunks().empty());
  EXPECT_EQ(1, fake_asr_input.audio_chunks().back()->channel_count);
  EXPECT_EQ(16000, fake_asr_input.audio_chunks().back()->sample_rate);
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);
  EXPECT_FALSE(error_occurred_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       HaltsAudioSendingWhenClientNotRequestingSpeechRecognition) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // 1. Initial recognition event responds with success = false (client no
  // longer requesting captions).
  recognition_event_success_response_ = false;
  SendAudio(recognizer.get(), base::Seconds(1));
  WaitForRecognitionEvent();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_asr_input.audio_chunks().empty());

  // 2. Clear sent audio chunks and attempt to send more audio.
  fake_asr_input.ClearAudioChunks();
  SendAudio(recognizer.get(), base::Seconds(1));
  task_environment_.RunUntilIdle();

  // 3. Audio should be dropped because is_client_requesting_speech_recognition_
  // is false.
  EXPECT_TRUE(fake_asr_input.audio_chunks().empty());
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       DestructorInvokesOnSpeechRecognitionStopped) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  EXPECT_FALSE(speech_recognition_stopped_called_);

  // Resetting recognizer triggers destructor which calls
  // OnSpeechRecognitionStopped on client.
  recognizer.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return speech_recognition_stopped_called_; }));
  EXPECT_TRUE(speech_recognition_stopped_called_);
  EXPECT_EQ(1, speech_recognition_stopped_count_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       DisconnectAsrStreamCallsStoppedOnlyOnceEvenAfterDestruction) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  auto fake_asr_input = std::make_unique<FakeAsrStreamInput>(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // Disconnect stream -> triggers error and stopped.
  fake_asr_input.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return speech_recognition_stopped_called_; }));
  EXPECT_EQ(1, speech_recognition_stopped_count_);

  // Destroying recognizer should NOT invoke OnSpeechRecognitionStopped a
  // second time.
  recognizer.reset();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1, speech_recognition_stopped_count_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       MarkDoneAllowsLateResponseFromAsrStreamResponder) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // Call MarkDone() -> closes asr_stream_input_ but leaves responder alive.
  recognizer->MarkDone();

  // Responder receives final transcription.
  std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> results;
  auto result = on_device_model::mojom::SpeechRecognitionResult::New();
  result->transcript = "Final flushed audio transcription";
  result->is_final = true;
  results.push_back(std::move(result));

  recognizer->OnResponse(std::move(results));
  WaitForRecognitionEvent();
  EXPECT_EQ("Final flushed audio transcription",
            last_received_result_.transcription);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       MarkDoneCleanDisconnectDoesNotTriggerError) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  auto fake_asr_input = std::make_unique<FakeAsrStreamInput>(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  SendAudio(recognizer.get(), base::Seconds(1));
  WaitForRecognitionEvent();
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);

  // Mark stream as done.
  recognizer->MarkDone();

  // Resetting ASR stream (model service closing the stream upon completion)
  // after MarkDone() must NOT trigger OnSpeechRecognitionError.
  fake_asr_input.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return speech_recognition_stopped_called_; }));
  EXPECT_FALSE(error_occurred_);
  EXPECT_TRUE(speech_recognition_stopped_called_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, ClientDisconnectCleansUpState) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  mojo::Remote<on_device_model::mojom::Session> session_remote;
  FakeSession fake_session(session_remote.BindNewPipeAndPassReceiver());

  bool session_disconnected = false;
  fake_session.set_disconnect_handler(base::BindOnce(
      [](bool* disconnected) { *disconnected = true; }, &session_disconnected));

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      std::move(session_remote), std::move(asr_stream_input),
      std::move(asr_stream_responder));

  // Disconnect the client receiver.
  receiver_.reset();
  ASSERT_TRUE(base::test::RunUntil([&]() { return session_disconnected; }));
  EXPECT_TRUE(session_disconnected);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       OnLanguageChangedMaintainsConnectionAcrossLanguages) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // Language changed to non-English (e.g. French, Spanish, Japanese).
  recognizer->OnLanguageChanged("fr-FR");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(speech_recognition_stopped_called_);
  EXPECT_FALSE(error_occurred_);

  recognizer->OnLanguageChanged("es-ES");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(speech_recognition_stopped_called_);
  EXPECT_FALSE(error_occurred_);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, SendAudioEarlyExitWhenMarkedDone) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  recognizer->MarkDone();
  fake_asr_input.ClearAudioChunks();

  // Sending audio after MarkDone() should early-exit immediately without adding
  // chunks.
  SendAudio(recognizer.get(), base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(fake_asr_input.audio_chunks().empty());
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, SendAudioEarlyExitWhenStopped) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // Trigger stopped by disconnecting the ASR stream.
  fake_asr_input.ResetReceiver();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return speech_recognition_stopped_called_; }));

  fake_asr_input.ClearAudioChunks();
  // Sending audio when stopped should early-exit without processing.
  SendAudio(recognizer.get(), base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(fake_asr_input.audio_chunks().empty());
}

TEST_F(OnDeviceLiveCaptionRecognizerTest, OnResponseHandlesMultipleResults) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  std::vector<on_device_model::mojom::SpeechRecognitionResultPtr> results;
  auto res1 = on_device_model::mojom::SpeechRecognitionResult::New();
  res1->transcript = "first partial";
  res1->is_final = false;
  results.push_back(std::move(res1));

  auto res2 = on_device_model::mojom::SpeechRecognitionResult::New();
  res2->transcript = "second final";
  res2->is_final = true;
  results.push_back(std::move(res2));

  recognizer->OnResponse(std::move(results));
  WaitForRecognitionEvent();
  EXPECT_EQ("first partial", last_received_result_.transcription);
  EXPECT_FALSE(last_received_result_.is_final);

  WaitForRecognitionEvent();
  EXPECT_EQ("second final", last_received_result_.transcription);
  EXPECT_TRUE(last_received_result_.is_final);
}

TEST_F(OnDeviceLiveCaptionRecognizerTest,
       OnLanguageChangedToEnglishKeepsConnection) {
  mojo::PendingRemote<on_device_model::mojom::AsrStreamInput> asr_stream_input;
  mojo::PendingReceiver<on_device_model::mojom::AsrStreamResponder>
      asr_stream_responder;

  FakeAsrStreamInput fake_asr_input(
      asr_stream_input.InitWithNewPipeAndPassReceiver(),
      asr_stream_responder.InitWithNewPipeAndPassRemote());

  auto recognizer = std::make_unique<OnDeviceLiveCaptionRecognizer>(
      receiver_.BindNewPipeAndPassRemote(), CreateOptions(),
      /*session=*/mojo::Remote<on_device_model::mojom::Session>(),
      std::move(asr_stream_input), std::move(asr_stream_responder));

  // Language changed to another English locale (e.g. en-GB) -> should not
  // disconnect.
  recognizer->OnLanguageChanged("en-GB");
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(speech_recognition_stopped_called_);

  SendAudio(recognizer.get(), base::Seconds(1));
  WaitForRecognitionEvent();
  EXPECT_EQ("OnDeviceModel transcription", last_received_result_.transcription);
}

}  // namespace speech
