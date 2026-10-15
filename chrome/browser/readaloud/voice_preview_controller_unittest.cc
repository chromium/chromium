// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/voice_preview_controller.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/readaloud/audio_generation/speech_synthesis_broker.h"
#include "chrome/browser/readaloud/fake_audio_stream_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"

namespace readaloud {

namespace {

constexpr std::string_view kTestVoiceId = "msf00006";
constexpr std::u16string_view kTestPreviewMessage =
    u"Here are the voices you can pick for reading out your web pages. If you "
    u"like this voice and want me to keep using it just tap the voice name.";

class FakePlaybackController
    : public read_aloud::mojom::ReadAloudPlaybackController {
 public:
  FakePlaybackController() = default;
  ~FakePlaybackController() override = default;

  void Bind(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver,
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
          client) {
    bind_count_++;
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&FakePlaybackController::OnReceiverDisconnected,
                       base::Unretained(this)));
    is_connected_ = true;
    client_.reset();
    client_.Bind(std::move(client));
  }

  void Reset() {
    receiver_.reset();
    is_connected_ = false;
    client_.reset();
    received_segments_.clear();
  }

  void FlushForTesting() {
    if (receiver_.is_bound()) {
      receiver_.FlushForTesting();
    }
    if (client_.is_bound()) {
      client_.FlushForTesting();
    }
  }

  void InitializeAudio(
      mojo::PendingRemote<media::mojom::AudioOutputStream> stream,
      media::mojom::ReadWriteAudioDataPipePtr data_pipe,
      const media::AudioParameters& params) override {
    initialize_audio_count_++;
  }

  void SetPlaybackMode(read_aloud::mojom::PlaybackMode mode) override {}

  void SetTextContent(
      std::vector<read_aloud::mojom::TextSegmentPtr> segments) override {
    received_segments_ = std::move(segments);
    if (client_.is_bound()) {
      client_->OnTextChunked(/*chunks=*/{});
    }
  }

  void SetOverviewContent(mojo_base::BigBuffer response_bytes,
                          SetOverviewContentCallback callback) override {
    std::move(callback).Run(/*success=*/false, /*title=*/std::string());
  }

  void Play() override {
    play_count_++;
    if (play_callback_) {
      std::move(play_callback_).Run();
    }
  }

  void Pause() override {
    pause_count_++;
    if (pause_callback_) {
      std::move(pause_callback_).Run();
    }
  }
  void SeekToWord(uint32_t segment_index, uint32_t character_offset) override {}
  void SeekToTime(base::TimeDelta position) override {}
  void SeekRelative(base::TimeDelta offset) override {}
  void SetVoice(const std::string& voice_id) override {}
  void SetPlaybackRate(float rate) override {}
  void FlushBuffers() override {}

  void set_play_callback(base::OnceClosure callback) {
    play_callback_ = std::move(callback);
  }
  void set_pause_callback(base::OnceClosure callback) {
    pause_callback_ = std::move(callback);
  }

  const std::vector<read_aloud::mojom::TextSegmentPtr>& received_segments()
      const {
    return received_segments_;
  }

  int bind_count() const { return bind_count_; }
  int initialize_audio_count() const { return initialize_audio_count_; }
  int play_count() const { return play_count_; }
  int pause_count() const { return pause_count_; }
  bool is_connected() const { return is_connected_; }

  read_aloud::mojom::ReadAloudPlaybackControllerClient* client() {
    return client_.get();
  }

 private:
  void OnReceiverDisconnected() { is_connected_ = false; }

  mojo::Receiver<read_aloud::mojom::ReadAloudPlaybackController> receiver_{
      this};
  mojo::Remote<read_aloud::mojom::ReadAloudPlaybackControllerClient> client_;
  std::vector<read_aloud::mojom::TextSegmentPtr> received_segments_;
  base::OnceClosure play_callback_;
  base::OnceClosure pause_callback_;
  int bind_count_ = 0;
  int initialize_audio_count_ = 0;
  int play_count_ = 0;
  int pause_count_ = 0;
  bool is_connected_ = false;
};

}  // namespace

class VoicePreviewControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitAndEnableFeature(features::kReadAloudNative);

    speech_synthesis_broker_ = std::make_unique<SpeechSynthesisBroker>();
    controller_ = std::make_unique<VoicePreviewController>(
        profile(), speech_synthesis_broker_.get(),
        base::BindRepeating(
            &testing::MockFunction<void(
                std::string_view, read_aloud::mojom::PlaybackState)>::Call,
            base::Unretained(&state_callback_)),
        base::BindRepeating(&VoicePreviewControllerTest::BindController,
                            base::Unretained(this)),
        base::BindRepeating(&VoicePreviewControllerTest::BindAudioStreamFactory,
                            base::Unretained(this)));
  }

  void TearDown() override {
    controller_.reset();
    speech_synthesis_broker_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  MockOptimizationGuideKeyedService* SetupMockOptimizationGuide() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating([](content::BrowserContext*)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<
                      testing::NiceMock<MockOptimizationGuideKeyedService>>();
                })));
  }

  VoicePreviewController* controller() { return controller_.get(); }
  SpeechSynthesisBroker* speech_synthesis_broker() {
    return speech_synthesis_broker_.get();
  }
  FakePlaybackController* fake_controller() { return &fake_controller_; }
  FakeAudioStreamFactory* fake_audio_stream_factory() {
    return &fake_audio_stream_factory_;
  }
  testing::MockFunction<void(std::string_view,
                             read_aloud::mojom::PlaybackState)>&
  state_callback() {
    return state_callback_;
  }

 private:
  void BindController(
      mojo::PendingReceiver<read_aloud::mojom::ReadAloudPlaybackController>
          receiver,
      mojo::PendingRemote<read_aloud::mojom::ReadAloudPlaybackControllerClient>
          client) {
    fake_controller_.Bind(std::move(receiver), std::move(client));
  }

  void BindAudioStreamFactory(
      mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
    fake_audio_stream_factory_.Bind(std::move(receiver));
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  FakePlaybackController fake_controller_;
  FakeAudioStreamFactory fake_audio_stream_factory_;
  testing::StrictMock<testing::MockFunction<
      void(std::string_view, read_aloud::mojom::PlaybackState)>>
      state_callback_;
  std::unique_ptr<SpeechSynthesisBroker> speech_synthesis_broker_;
  std::unique_ptr<VoicePreviewController> controller_;
};

TEST_F(VoicePreviewControllerTest,
       PlayPreviewSendsSampleTextAndCompletesFullLifecycle) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);

  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  EXPECT_TRUE(controller()->IsPreviewActive());
  fake_controller()->FlushForTesting();

  ASSERT_EQ(fake_controller()->received_segments().size(), 1u);
  EXPECT_EQ(fake_controller()->received_segments()[0]->segment_index, 0u);
  EXPECT_EQ(fake_controller()->received_segments()[0]->text,
            std::u16string(kTestPreviewMessage));
  EXPECT_EQ(fake_controller()->play_count(), 1);

  // Utility reports kPlaying once synthesis of the sample begins playing.
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();

  // Mid-playback kBuffering and kPaused must be forwarded without resetting the
  // active preview.
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kBuffering);
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();
  EXPECT_TRUE(controller()->IsPreviewActive());

  // Utility reports kEndOfStream when the sample finishes draining, which
  // translates to kStopped for the voice preview while retaining the utility
  // connection so buffered audio can finish playing out.
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kEndOfStream);
  fake_controller()->FlushForTesting();
  EXPECT_FALSE(controller()->IsPreviewActive());
  EXPECT_TRUE(fake_controller()->is_connected());

  // Calling StopPreview() after natural completion is a no-op so buffered tail
  // audio can finish playing out without tearing down the connection or
  // emitting a duplicate kStopped callback.
  controller()->StopPreview();
  fake_controller()->FlushForTesting();
  EXPECT_TRUE(fake_controller()->is_connected());

  // Explicit Reset() tears down the retained connection.
  controller()->Reset();
  fake_controller()->FlushForTesting();
  EXPECT_FALSE(fake_controller()->is_connected());
}

TEST_F(VoicePreviewControllerTest,
       SwitchingActivePreviewCreatesNewControllerAndAudioStream) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller()->initialize_audio_count() == 1; }));
  EXPECT_EQ(fake_controller()->bind_count(), 1);
  EXPECT_EQ(fake_audio_stream_factory()->create_output_stream_called_count(),
            1);

  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();

  // Stopping and starting a second preview (matching ReadAloudController's
  // destroyVoicePreview() + createNativeVoicePreviewPlayback() sequence) tears
  // down the previous controller/audio stream and binds fresh ones for the new
  // preview.
  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  controller()->StopPreview();
  fake_controller()->FlushForTesting();
  EXPECT_FALSE(fake_controller()->is_connected());

  constexpr std::u16string_view kSpanishPreviewMessage =
      u"Hola, esta es una muestra de voz para leer tus textos web.";
  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"second-voice-id",
                   read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(/*voice_id=*/"second-voice-id",
                            kSpanishPreviewMessage);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller()->initialize_audio_count() == 2; }));
  EXPECT_TRUE(controller()->IsPreviewActive());
  EXPECT_EQ(fake_controller()->bind_count(), 2);
  EXPECT_EQ(fake_audio_stream_factory()->create_output_stream_called_count(),
            2);
  EXPECT_EQ(fake_controller()->play_count(), 2);
  ASSERT_EQ(fake_controller()->received_segments().size(), 1u);
  EXPECT_EQ(fake_controller()->received_segments()[0]->text,
            std::u16string(kSpanishPreviewMessage));

  EXPECT_CALL(state_callback(), Call(/*voice_id=*/"second-voice-id",
                                     read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();
  EXPECT_TRUE(controller()->IsPreviewActive());
}

TEST_F(VoicePreviewControllerTest,
       StopPreviewCancelsActivePreviewAndEmitsStopped) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  EXPECT_TRUE(controller()->IsPreviewActive());

  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  controller()->StopPreview();
  fake_controller()->FlushForTesting();
  EXPECT_FALSE(controller()->IsPreviewActive());
  EXPECT_FALSE(fake_controller()->is_connected());

  // Calling StopPreview() again when no preview is active should be a no-op.
  controller()->StopPreview();
}

TEST_F(VoicePreviewControllerTest,
       PauseAndResumePreviewReusesActiveController) {
  // Calling PausePreview() when no preview is active should be a no-op.
  controller()->PausePreview();
  EXPECT_EQ(fake_controller()->pause_count(), 0);

  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller()->initialize_audio_count() == 1; }));
  EXPECT_EQ(fake_controller()->bind_count(), 1);
  EXPECT_EQ(fake_controller()->play_count(), 1);

  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();

  // Pause the active preview.
  base::RunLoop pause_loop;
  fake_controller()->set_pause_callback(pause_loop.QuitClosure());
  controller()->PausePreview();
  pause_loop.Run();
  EXPECT_EQ(fake_controller()->pause_count(), 1);

  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kPaused))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->FlushForTesting();
  EXPECT_TRUE(controller()->IsPreviewActive());

  // Calling PlayPreview() with the same voice_id resumes on the existing
  // controller and audio stream without re-emitting kBuffering.
  base::RunLoop resume_loop;
  fake_controller()->set_play_callback(resume_loop.QuitClosure());
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  resume_loop.Run();
  EXPECT_EQ(fake_controller()->bind_count(), 1);
  EXPECT_EQ(fake_audio_stream_factory()->create_output_stream_called_count(),
            1);
  EXPECT_EQ(fake_controller()->play_count(), 2);

  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();
}

TEST_F(VoicePreviewControllerTest,
       RequestSpeechSynthesisUsesPreviewVoiceOverrideWithoutMutatingBroker) {
  MockOptimizationGuideKeyedService* mock_opt_guide =
      SetupMockOptimizationGuide();

  speech_synthesis_broker()->SetVoice(/*voice_id=*/"configured-article-voice");

  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"preview-voice-id",
                   read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(/*voice_id=*/"preview-voice-id",
                            kTestPreviewMessage);

  // Empty text_chunk must be rejected before calling into the broker.
  base::test::TestFuture<mojo_base::BigBuffer, bool> empty_future;
  controller()->RequestSpeechSynthesis(
      /*text_chunk=*/u"", read_aloud::mojom::Speaker::kSpeaker1,
      /*sequence_id=*/0, empty_future.GetCallback());
  EXPECT_FALSE(empty_future.Get<bool>());

  optimization_guide::proto::Any any;
  any.set_value("fake_audio_bytes");

  // 1. Synthesis requested during voice preview must use "preview-voice-id".
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&any](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& synthesize_request = static_cast<
                const optimization_guide::proto::ReadAloudSynthesizeRequest&>(
                request_metadata);
            EXPECT_EQ(synthesize_request.voice_id(), "preview-voice-id");

            optimization_guide::OptimizationGuideModelExecutionResult result(
                any, /*execution_info=*/nullptr);
            std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
          });

  base::test::TestFuture<mojo_base::BigBuffer, bool> preview_future;
  controller()->RequestSpeechSynthesis(std::u16string(kTestPreviewMessage),
                                       read_aloud::mojom::Speaker::kSpeaker1,
                                       /*sequence_id=*/1,
                                       preview_future.GetCallback());
  EXPECT_TRUE(preview_future.Get<bool>());
  EXPECT_EQ(base::as_string_view(preview_future.Get<mojo_base::BigBuffer>()),
            "fake_audio_bytes");

  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  controller()->StopPreview();

  // Synthesis requested when no preview is active must fail without calling
  // ExecuteModel.
  base::test::TestFuture<mojo_base::BigBuffer, bool> inactive_future;
  controller()->RequestSpeechSynthesis(std::u16string(kTestPreviewMessage),
                                       read_aloud::mojom::Speaker::kSpeaker1,
                                       /*sequence_id=*/2,
                                       inactive_future.GetCallback());
  EXPECT_FALSE(inactive_future.Get<bool>());

  // 2. Article synthesis on the broker must still use
  // "configured-article-voice".
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&any](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) {
            const auto& synthesize_request = static_cast<
                const optimization_guide::proto::ReadAloudSynthesizeRequest&>(
                request_metadata);
            EXPECT_EQ(synthesize_request.voice_id(),
                      "configured-article-voice");

            optimization_guide::OptimizationGuideModelExecutionResult result(
                any, /*execution_info=*/nullptr);
            std::move(callback).Run(std::move(result), /*log_entry=*/nullptr);
          });

  base::test::TestFuture<mojo_base::BigBuffer, bool> article_future;
  speech_synthesis_broker()->SynthesizeSpeech(
      mock_opt_guide, /*text_chunk=*/u"Article sentence",
      article_future.GetCallback());
  EXPECT_TRUE(article_future.Get<bool>());
}

TEST_F(VoicePreviewControllerTest, PlayPreviewRejectsEmptyVoiceId) {
  testing::InSequence s;
  // Start a valid preview first to verify that a subsequent empty voice_id
  // cancels the active preview session.
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  EXPECT_TRUE(controller()->IsPreviewActive());

  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"", read_aloud::mojom::PlaybackState::kError))
      .Times(1);
  controller()->PlayPreview(/*voice_id=*/"", kTestPreviewMessage);
  EXPECT_FALSE(controller()->IsPreviewActive());
}

TEST_F(VoicePreviewControllerTest, PlayPreviewRejectsEmptyPreviewText) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  EXPECT_TRUE(controller()->IsPreviewActive());

  // Empty preview_text cancels active preview for a different voice and emits
  // kError.
  EXPECT_CALL(state_callback(), Call(/*voice_id=*/"other-voice",
                                     read_aloud::mojom::PlaybackState::kError))
      .Times(1);
  controller()->PlayPreview(/*voice_id=*/"other-voice", /*preview_text=*/u"");
  EXPECT_FALSE(controller()->IsPreviewActive());
}

TEST_F(VoicePreviewControllerTest, UtilityErrorEmitsPreviewErrorState) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);

  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kError))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kError);
  fake_controller()->FlushForTesting();
  EXPECT_FALSE(controller()->IsPreviewActive());
}

TEST_F(VoicePreviewControllerTest, UtilityDisconnectEmitsPreviewErrorState) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);

  base::RunLoop error_loop;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kError))
      .WillOnce([&]() { error_loop.Quit(); });
  fake_controller()->Reset();
  error_loop.Run();
  EXPECT_FALSE(controller()->IsPreviewActive());
}

TEST_F(VoicePreviewControllerTest,
       AudioStreamCreationFailureEmitsPreviewErrorState) {
  fake_audio_stream_factory()->set_auto_respond(/*auto_respond=*/true,
                                                /*should_succeed=*/false);

  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);

  base::RunLoop error_loop;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kError))
      .WillOnce([&]() { error_loop.Quit(); });

  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  error_loop.Run();
  EXPECT_FALSE(controller()->IsPreviewActive());
}

TEST_F(VoicePreviewControllerTest,
       OnPlaybackStateChangedRejectsBrowserOnlyStates) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);

  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kError))
      .Times(1);
  mojo::test::BadMessageObserver bad_message_observer;
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kStopped);

  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "VoicePreviewController: browser-only PlaybackState received from "
            "utility");
  EXPECT_FALSE(controller()->IsPreviewActive());
}

TEST_F(VoicePreviewControllerTest,
       SwitchingActivePreviewDropsStaleInFlightStatesFromPreviousPipe) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller()->initialize_audio_count() == 1; }));

  // Queue a stale kPlaying, kPaused, and kError from the first preview on the
  // first client pipe before switching to a second preview.
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPaused);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kError);

  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"second-voice-id",
                   read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(/*voice_id=*/"second-voice-id",
                            kTestPreviewMessage);

  // Because PlayPreview() resets the previous client receiver and binds a fresh
  // pipe, the queued stale messages from the first preview are dropped and
  // "second-voice-id" remains active.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller()->initialize_audio_count() == 2; }));
  EXPECT_TRUE(controller()->IsPreviewActive());
  EXPECT_EQ(fake_controller()->bind_count(), 2);

  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"second-voice-id",
                   read_aloud::mojom::PlaybackState::kPlaying))
      .Times(1);
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();
}

TEST_F(VoicePreviewControllerTest,
       StopPreviewCancelsInFlightSpeechSynthesisCallback) {
  MockOptimizationGuideKeyedService* mock_opt_guide =
      SetupMockOptimizationGuide();

  EXPECT_CALL(state_callback(),
              Call(kTestVoiceId, read_aloud::mojom::PlaybackState::kBuffering))
      .Times(1);
  controller()->PlayPreview(kTestVoiceId, kTestPreviewMessage);

  optimization_guide::OptimizationGuideModelExecutionResultCallback
      held_model_callback;
  EXPECT_CALL(
      *mock_opt_guide,
      ExecuteModel(
          optimization_guide::ModelBasedCapabilityKey::kReadAloudSynthesize,
          testing::_, testing::_, testing::_))
      .WillOnce(
          [&held_model_callback](
              optimization_guide::ModelBasedCapabilityKey feature,
              const google::protobuf::MessageLite& request_metadata,
              const optimization_guide::ModelExecutionOptions& options,
              optimization_guide::OptimizationGuideModelExecutionResultCallback
                  callback) { held_model_callback = std::move(callback); });

  base::test::TestFuture<mojo_base::BigBuffer, bool> synthesis_future;
  controller()->RequestSpeechSynthesis(std::u16string(kTestPreviewMessage),
                                       read_aloud::mojom::Speaker::kSpeaker1,
                                       /*sequence_id=*/1,
                                       synthesis_future.GetCallback());
  ASSERT_TRUE(held_model_callback);
  EXPECT_FALSE(synthesis_future.IsReady());

  // Stopping the preview while ExecuteModel is in flight must invalidate
  // `preview_weak_factory_`, resolving the Mojo callback with `success = false`
  // when the model execution callback later completes.
  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"", read_aloud::mojom::PlaybackState::kStopped))
      .Times(1);
  controller()->StopPreview();

  optimization_guide::proto::Any any;
  any.set_value("stale_audio_bytes");
  optimization_guide::OptimizationGuideModelExecutionResult result(
      any, /*execution_info=*/nullptr);
  std::move(held_model_callback).Run(std::move(result), /*log_entry=*/nullptr);

  EXPECT_FALSE(synthesis_future.Get<bool>());
  EXPECT_THAT(
      base::as_string_view(synthesis_future.Get<mojo_base::BigBuffer>()),
      testing::IsEmpty());
}

TEST_F(VoicePreviewControllerTest,
       RapidConsecutivePlayPreviewWhileBufferingAbortsEarlierRequests) {
  testing::InSequence s;
  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"voice-1",
                   read_aloud::mojom::PlaybackState::kBuffering));
  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"voice-2",
                   read_aloud::mojom::PlaybackState::kBuffering));
  EXPECT_CALL(state_callback(),
              Call(/*voice_id=*/"voice-3",
                   read_aloud::mojom::PlaybackState::kBuffering));

  controller()->PlayPreview(/*voice_id=*/"voice-1", kTestPreviewMessage);
  controller()->PlayPreview(/*voice_id=*/"voice-2", kTestPreviewMessage);
  controller()->PlayPreview(/*voice_id=*/"voice-3", kTestPreviewMessage);

  // Earlier pending audio stream creations were aborted; only "voice-3"
  // initializes audio and transitions to kPlaying.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller()->initialize_audio_count() == 1; }));
  EXPECT_EQ(fake_controller()->bind_count(), 3);
  EXPECT_TRUE(controller()->IsPreviewActive());

  EXPECT_CALL(
      state_callback(),
      Call(/*voice_id=*/"voice-3", read_aloud::mojom::PlaybackState::kPlaying));
  fake_controller()->client()->OnPlaybackStateChanged(
      read_aloud::mojom::PlaybackState::kPlaying);
  fake_controller()->FlushForTesting();
}

}  // namespace readaloud
