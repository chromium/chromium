// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/hotl/soda_hotl_speech_recognizer.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

class FakeSpeechRecognitionService
    : public media::mojom::SpeechRecognitionContext,
      public media::mojom::SpeechRecognitionRecognizer {
 public:
  FakeSpeechRecognitionService() = default;
  ~FakeSpeechRecognitionService() override = default;

  void BindContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
    context_receiver_.reset();
    context_receiver_.Bind(std::move(receiver));
  }

  // media::mojom::SpeechRecognitionContext:
  void BindRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer> receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionRecognizerClient>
          client,
      media::mojom::SpeechRecognitionOptionsPtr options,
      BindRecognizerCallback callback) override {
    recognizer_receiver_.reset();
    recognizer_receiver_.Bind(std::move(receiver));
    client_remote_.reset();
    client_remote_.Bind(std::move(client));
    last_options_ = std::move(options);
    ++bind_recognizer_count_;
    std::move(callback).Run(/*is_multichannel_supported=*/false);
  }

  void BindWebSpeechRecognizer(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          session_client,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionAudioForwarder>
          audio_forwarder,
      int32_t channel_count,
      int32_t sample_rate,
      media::mojom::SpeechRecognitionOptionsPtr options,
      bool continuous) override {}

  // media::mojom::SpeechRecognitionRecognizer:
  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr buffer,
      std::optional<base::TimeDelta> media_start_pts) override {
    received_buffers_.push_back(std::move(buffer));
  }

  void MarkDone() override {}
  void OnLanguageChanged(const std::string& language) override {}
  void OnMaskOffensiveWordsChanged(bool mask_offensive_words) override {}
  void UpdateRecognitionContext(
      const media::SpeechRecognitionRecognitionContext& recognition_context)
      override {}

  void SendRecognitionEvent(const std::string& transcription, bool is_final) {
    base::test::TestFuture<bool> reply;
    QueueRecognitionEvent(transcription, is_final, reply.GetCallback());
    last_continue_reply_ = reply.Get();
  }

  void QueueRecognitionEvent(
      const std::string& transcription,
      bool is_final,
      media::mojom::SpeechRecognitionRecognizerClient::
          OnSpeechRecognitionRecognitionEventCallback callback) {
    client_remote_->OnSpeechRecognitionRecognitionEvent(
        media::SpeechRecognitionResult(transcription, is_final),
        std::move(callback));
  }

  void SendError() {
    client_remote_->OnSpeechRecognitionError();
    client_remote_.FlushForTesting();
  }

  void SendStopped() {
    client_remote_->OnSpeechRecognitionStopped();
    client_remote_.FlushForTesting();
  }

  void DisconnectRecognizer() {
    recognizer_receiver_.reset();
    client_remote_.reset();
  }

  void FlushClientForTesting() {
    if (client_remote_.is_bound()) {
      client_remote_.FlushForTesting();
    }
  }

  int bind_recognizer_count() const { return bind_recognizer_count_; }

  bool is_client_connected() const {
    return client_remote_.is_bound() && client_remote_.is_connected();
  }

  const media::mojom::SpeechRecognitionOptions* last_options() const {
    return last_options_.get();
  }

  const std::vector<media::mojom::AudioDataS16Ptr>& received_buffers() const {
    return received_buffers_;
  }

  std::optional<bool> last_continue_reply() const {
    return last_continue_reply_;
  }

 private:
  mojo::Receiver<media::mojom::SpeechRecognitionContext> context_receiver_{
      this};
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizer>
      recognizer_receiver_{this};
  mojo::Remote<media::mojom::SpeechRecognitionRecognizerClient> client_remote_;
  media::mojom::SpeechRecognitionOptionsPtr last_options_;
  int bind_recognizer_count_ = 0;
  std::vector<media::mojom::AudioDataS16Ptr> received_buffers_;
  std::optional<bool> last_continue_reply_;
};

class SodaHotlSpeechRecognizerTest : public testing::Test {
 protected:
  SodaHotlSpeechRecognizer CreateRecognizer() {
    return SodaHotlSpeechRecognizer(base::BindRepeating(
        &FakeSpeechRecognitionService::BindContext, base::Unretained(&fake_)));
  }

  // Starts `recognizer` and waits until the fake service binds it.
  void StartAndWait(SodaHotlSpeechRecognizer& recognizer,
                    HotlSpeechRecognizer::TranscriptCallback on_transcript,
                    HotlSpeechRecognizer::ErrorCallback on_error,
                    std::string_view locale = "en-US") {
    const int binds = fake_.bind_recognizer_count();
    recognizer.Start(locale, std::move(on_transcript), std::move(on_error));
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return fake_.bind_recognizer_count() == binds + 1; }));
  }

  // Runs every task already posted to the current sequence.
  void FlushPostedTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  const media::AudioParameters mono_params_{
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), /*sample_rate=*/16000,
      /*frames_per_buffer=*/4};

  base::test::SingleThreadTaskEnvironment task_environment_;
  FakeSpeechRecognitionService fake_;
};

TEST_F(SodaHotlSpeechRecognizerTest,
       BindsRecognizerAndForwardsAudioChunksToSoda) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();
  StartAndWait(recognizer, base::DoNothing(), base::DoNothing());

  ASSERT_NE(fake_.last_options(), nullptr);
  EXPECT_EQ(fake_.last_options()->language, "en-US");
  EXPECT_FALSE(fake_.last_options()->is_server_based);
  EXPECT_FALSE(fake_.last_options()->allow_multi_language);
  EXPECT_EQ(fake_.last_options()->recognition_mode,
            media::mojom::SpeechRecognitionMode::kCaption);
  EXPECT_EQ(fake_.last_options()->recognizer_client_type,
            media::mojom::RecognizerClientType::kLiveCaption);

  // Empty spans are ignored before valid audio arrives.
  recognizer.PushAudioChunk({}, mono_params_);

  const int16_t pcm_samples[] = {100, -200, 300, -400};
  recognizer.PushAudioChunk(pcm_samples, mono_params_);
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !fake_.received_buffers().empty(); }));

  ASSERT_EQ(fake_.received_buffers().size(), 1u);
  const auto& buffer = fake_.received_buffers()[0];
  EXPECT_EQ(buffer->channel_count, 1);
  EXPECT_EQ(buffer->sample_rate, 16000);
  EXPECT_EQ(buffer->frame_count, 4);
  EXPECT_THAT(buffer->data, ElementsAre(100, -200, 300, -400));
}

TEST_F(SodaHotlSpeechRecognizerTest, IgnoresAudioChunksWhenNotActiveOrEmpty) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();
  const int16_t even[] = {1, 2, 3, 4};
  // Not started yet: ignored without crash.
  recognizer.PushAudioChunk(even, mono_params_);

  StartAndWait(recognizer, base::DoNothing(), base::DoNothing());
  const media::AudioParameters stereo(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Stereo(), /*sample_rate=*/16000,
      /*frames_per_buffer=*/2);
  // Empty span: ignored.
  recognizer.PushAudioChunk({}, stereo);
  // Valid stereo chunk: forwarded.
  recognizer.PushAudioChunk(even, stereo);
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !fake_.received_buffers().empty(); }));

  ASSERT_EQ(fake_.received_buffers().size(), 1u);
  EXPECT_EQ(fake_.received_buffers()[0]->channel_count, 2);
  EXPECT_EQ(fake_.received_buffers()[0]->frame_count, 2);

  // After Stop: ignored without crash.
  recognizer.Stop();
  recognizer.PushAudioChunk(even, stereo);
  FlushPostedTasks();
  EXPECT_EQ(fake_.received_buffers().size(), 1u);
}

TEST_F(SodaHotlSpeechRecognizerTest,
       IgnoresPartialTranscriptsAndEmitsFinalizedTranscripts) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();

  std::vector<std::u16string> final_transcripts;
  bool error_called = false;
  StartAndWait(recognizer,
               base::BindLambdaForTesting([&](std::u16string_view transcript) {
                 final_transcripts.emplace_back(transcript);
               }),
               base::BindLambdaForTesting([&]() { error_called = true; }));

  fake_.SendRecognitionEvent("ye", /*is_final=*/false);
  EXPECT_EQ(fake_.last_continue_reply(), true);
  EXPECT_THAT(final_transcripts, IsEmpty());

  fake_.SendRecognitionEvent("", /*is_final=*/true);
  fake_.SendRecognitionEvent("wait", /*is_final=*/true);
  fake_.SendRecognitionEvent("yes", /*is_final=*/true);
  fake_.SendRecognitionEvent("S\xc3\xad", /*is_final=*/true);
  fake_.SendRecognitionEvent("\xe3\x81\xaf\xe3\x81\x84", /*is_final=*/true);
  EXPECT_EQ(fake_.last_continue_reply(), true);
  EXPECT_THAT(final_transcripts,
              ElementsAre(u"wait", u"yes", u"S\u00ed", u"\u306f\u3044"));
  EXPECT_FALSE(error_called);
}

TEST_F(SodaHotlSpeechRecognizerTest, TranscriptCallbackCanCallStop) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();
  int call_count = 0;
  StartAndWait(recognizer, base::BindLambdaForTesting([&](std::u16string_view) {
                 ++call_count;
                 recognizer.Stop();
               }),
               base::DoNothing());

  fake_.SendRecognitionEvent("yes", /*is_final=*/true);
  EXPECT_EQ(call_count, 1);
  fake_.FlushClientForTesting();
  EXPECT_FALSE(fake_.is_client_connected());
}

TEST_F(SodaHotlSpeechRecognizerTest, TranscriptCallbackCanDestroyRecognizer) {
  auto recognizer = std::make_unique<SodaHotlSpeechRecognizer>(
      base::BindRepeating(&FakeSpeechRecognitionService::BindContext,
                          base::Unretained(&fake_)));
  int call_count = 0;
  StartAndWait(*recognizer,
               base::BindLambdaForTesting([&](std::u16string_view) {
                 ++call_count;
                 recognizer.reset();
               }),
               base::DoNothing());

  fake_.SendRecognitionEvent("yes", /*is_final=*/true);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(recognizer, nullptr);
}

TEST_F(SodaHotlSpeechRecognizerTest, DropsTranscriptsAfterStop) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();
  bool called = false;
  StartAndWait(recognizer, base::BindLambdaForTesting([&](std::u16string_view) {
                 called = true;
               }),
               base::DoNothing());

  bool reply_invoked = false;
  fake_.QueueRecognitionEvent(
      "yes", /*is_final=*/true,
      base::BindLambdaForTesting([&](bool) { reply_invoked = true; }));
  recognizer.Stop();
  ASSERT_TRUE(
      base::test::RunUntil([&] { return !fake_.is_client_connected(); }));
  FlushPostedTasks();
  EXPECT_FALSE(reply_invoked);
  EXPECT_FALSE(called);
}

TEST_F(SodaHotlSpeechRecognizerTest, ErrorsWhenServiceDropsContext) {
  SodaHotlSpeechRecognizer recognizer{base::BindRepeating(
      [](mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>) {})};
  base::test::TestFuture<void> error;
  recognizer.Start("en-US", base::DoNothing(), error.GetCallback());
  EXPECT_TRUE(error.Wait());
}

TEST_F(SodaHotlSpeechRecognizerTest,
       InvokesErrorCallbackOnServiceErrorOrDisconnect) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();

  base::test::TestFuture<void> error_future;
  StartAndWait(recognizer, base::DoNothing(), error_future.GetCallback());
  fake_.SendError();
  EXPECT_TRUE(error_future.Wait());

  base::test::TestFuture<void> stopped_future;
  StartAndWait(recognizer, base::DoNothing(), stopped_future.GetCallback());
  fake_.SendStopped();
  EXPECT_TRUE(stopped_future.Wait());

  // Both pipes disconnect, but `on_error` runs once.
  int errors = 0;
  StartAndWait(recognizer, base::DoNothing(),
               base::BindLambdaForTesting([&] { ++errors; }));
  fake_.DisconnectRecognizer();
  ASSERT_TRUE(base::test::RunUntil([&] { return errors > 0; }));
  FlushPostedTasks();
  EXPECT_EQ(errors, 1);
}

TEST_F(SodaHotlSpeechRecognizerTest, DestroyingWhileActiveRunsNoError) {
  int errors = 0;
  {
    SodaHotlSpeechRecognizer recognizer = CreateRecognizer();
    StartAndWait(recognizer, base::DoNothing(),
                 base::BindLambdaForTesting([&] { ++errors; }));
  }
  FlushPostedTasks();
  EXPECT_EQ(errors, 0);
}

TEST_F(SodaHotlSpeechRecognizerTest, EmptyLocaleOmitsLanguageOption) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();
  StartAndWait(recognizer, base::DoNothing(), base::DoNothing(),
               /*locale=*/"");

  ASSERT_NE(fake_.last_options(), nullptr);
  EXPECT_FALSE(fake_.last_options()->language.has_value());
  EXPECT_FALSE(fake_.last_options()->allow_multi_language);
}

TEST_F(SodaHotlSpeechRecognizerTest, StopThenStartResetsSessionCleanly) {
  SodaHotlSpeechRecognizer recognizer = CreateRecognizer();
  bool first_transcript_called = false;
  int first_errors = 0;
  StartAndWait(recognizer, base::BindLambdaForTesting([&](std::u16string_view) {
                 first_transcript_called = true;
               }),
               base::BindLambdaForTesting([&] { ++first_errors; }), "en-US");

  // Explicit Stop() required before starting a new session.
  recognizer.Stop();

  std::vector<std::u16string> second_transcripts;
  int second_errors = 0;
  StartAndWait(recognizer,
               base::BindLambdaForTesting([&](std::u16string_view transcript) {
                 second_transcripts.emplace_back(transcript);
               }),
               base::BindLambdaForTesting([&] { ++second_errors; }), "es-ES");

  FlushPostedTasks();
  EXPECT_EQ(first_errors, 0);
  EXPECT_EQ(second_errors, 0);
  EXPECT_EQ(fake_.last_options()->language, "es-ES");

  fake_.SendRecognitionEvent("S\xc3\xad", /*is_final=*/true);
  EXPECT_FALSE(first_transcript_called);
  EXPECT_THAT(second_transcripts, ElementsAre(u"S\u00ed"));
}

}  // namespace
}  // namespace ttc
