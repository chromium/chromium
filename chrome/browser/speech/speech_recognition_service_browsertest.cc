// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/sync_socket.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/chrome_speech_recognition_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/speech/speech_recognition_recognizer_impl.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/soda/pref_names.h"
#include "content/public/browser/audio_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#include "sandbox/policy/switches.h"
#include "services/audio/public/cpp/fake_stream_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::StrictMock;

namespace speech {

constexpr base::FilePath::CharType kSodaResourcesDir[] =
    FILE_PATH_LITERAL("third_party/soda/resources");

constexpr base::FilePath::CharType kSodaLanguagePackRelativePath[] =
    FILE_PATH_LITERAL("en_us");

constexpr base::FilePath::CharType kSodaTestAudioRelativePath[] =
    FILE_PATH_LITERAL("hey_google.wav");

constexpr int kExpectedChannelCount = 1;

constexpr base::FilePath::CharType kSodaBinaryRelativePath[] =
    FILE_PATH_LITERAL("libsoda_for_testing.so");

// TODO: Should be a way to generate this, this seems way too brittle.
const size_t kShMemSize = 82240;

class MockStream : public media::mojom::AudioInputStream {
 public:
  MOCK_METHOD0(Record, void());
  MOCK_METHOD1(SetVolume, void(double));
};

class TestStreamFactory : public audio::FakeStreamFactory {
 public:
  TestStreamFactory() : stream_(), stream_receiver_(&stream_) {}
  ~TestStreamFactory() override = default;
  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      bool enable_agc,
      base::ReadOnlySharedMemoryRegion key_press_count_buffer,
      CreateInputStreamCallback created_callback) {
    device_id_ = device_id;
    params_ = params;
    if (stream_receiver_.is_bound())
      stream_receiver_.reset();
    stream_receiver_.Bind(std::move(stream_receiver));
    if (client_)
      client_.reset();
    // Keep the passed client alive to avoid binding errors.
    client_.Bind(std::move(client));
    base::SyncSocket socket1, socket2;
    base::SyncSocket::CreatePair(&socket1, &socket2);
    std::move(created_callback)
        .Run({base::in_place,
              base::ReadOnlySharedMemoryRegion::Create(kShMemSize).region,
              mojo::PlatformHandle(socket1.Take())},
             false /*initially muted*/, base::UnguessableToken::Create());
  }

  mojo::PendingRemote<media::mojom::AudioStreamFactory> MakeRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitToCreateInputStream() {
    if (stream_receiver_.is_bound())
      return;
    base::RepeatingTimer check_timer;
    check_timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(10), this,
                      &TestStreamFactory::OnTimer);
    runner_.Run();
  }

  StrictMock<MockStream> stream_;
  mojo::Remote<media::mojom::AudioInputStreamClient> client_;
  mojo::Receiver<media::mojom::AudioInputStream> stream_receiver_;
  std::string device_id_;
  base::Optional<media::AudioParameters> params_;

 private:
  void OnTimer() {
    if (stream_receiver_.is_bound())
      runner_.Quit();
  }

  base::RunLoop runner_;
};

class SpeechRecognitionServiceTest
    : public InProcessBrowserTest,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  SpeechRecognitionServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        {media::kLiveCaption, media::kUseSodaForLiveCaption}, {});
  }
  ~SpeechRecognitionServiceTest() override = default;

  // InProcessBrowserTest
  void SetUp() override;

  // media::mojom::SpeechRecognitionRecognizerClient
  void OnSpeechRecognitionRecognitionEvent(
      media::mojom::SpeechRecognitionResultPtr result) override;
  void OnSpeechRecognitionError() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Required for the utility process to access the directory containing the
    // test files.
    command_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);
  }

 protected:
  void LaunchService();
  void LaunchServiceWithAudioSourceFetcher();

  // The root directory for test files.
  base::FilePath test_data_dir_;

  base::test::ScopedFeatureList scoped_feature_list_;
  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;

  mojo::Remote<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer_;

  mojo::Remote<media::mojom::AudioSourceFetcher> audio_source_fetcher_;

  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_client_receiver_{this};

  std::vector<std::string> recognition_results_;

  DISALLOW_COPY_AND_ASSIGN(SpeechRecognitionServiceTest);
};

void SpeechRecognitionServiceTest::SetUp() {
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir_));
  InProcessBrowserTest::SetUp();
}

void SpeechRecognitionServiceTest::OnSpeechRecognitionRecognitionEvent(
    media::mojom::SpeechRecognitionResultPtr result) {
  recognition_results_.push_back(std::move(result->transcription));
}

void SpeechRecognitionServiceTest::OnSpeechRecognitionError() {
  NOTREACHED();
}

void SpeechRecognitionServiceTest::OnLanguageIdentificationEvent(
    media::mojom::LanguageIdentificationEventPtr event) {
  NOTREACHED();
}

void SpeechRecognitionServiceTest::LaunchService() {
  // Launch the Speech Recognition service.
  auto* browser_context =
      static_cast<content::BrowserContext*>(browser()->profile());
  auto* service = new ChromeSpeechRecognitionService(browser_context);

  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  service->Create(std::move(speech_recognition_context_receiver));

  mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizer>
      pending_recognizer_receiver =
          speech_recognition_recognizer_.BindNewPipeAndPassReceiver();

  bool is_multichannel_supported = true;
  auto run_loop = std::make_unique<base::RunLoop>();
  // Bind the recognizer pipes used to send audio and receive results.
  speech_recognition_context_->BindRecognizer(
      std::move(pending_recognizer_receiver),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(
          [](bool* p_is_multichannel_supported, base::RunLoop* run_loop,
             bool is_multichannel_supported) {
            *p_is_multichannel_supported = is_multichannel_supported;
            run_loop->Quit();
          },
          &is_multichannel_supported, run_loop.get()));

  run_loop->Run();
  ASSERT_FALSE(is_multichannel_supported);
}

void SpeechRecognitionServiceTest::LaunchServiceWithAudioSourceFetcher() {
  // Launch the Speech Recognition service.
  auto* browser_context =
      static_cast<content::BrowserContext*>(browser()->profile());
  auto* service = new ChromeSpeechRecognitionService(browser_context);

  mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_receiver =
          speech_recognition_context_.BindNewPipeAndPassReceiver();
  service->Create(std::move(speech_recognition_context_receiver));

  bool is_multichannel_supported = true;
  auto run_loop = std::make_unique<base::RunLoop>();
  // Bind the recognizer pipes used to send audio and receive results.
  speech_recognition_context_->BindAudioSourceFetcher(
      audio_source_fetcher_.BindNewPipeAndPassReceiver(),
      speech_recognition_client_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(
          [](bool* p_is_multichannel_supported, base::RunLoop* run_loop,
             bool is_multichannel_supported) {
            *p_is_multichannel_supported = is_multichannel_supported;
            run_loop->Quit();
          },
          &is_multichannel_supported, run_loop.get()));
  run_loop->Run();
  ASSERT_FALSE(is_multichannel_supported);
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionServiceTest, RecognizePhrase) {
  base::HistogramTester histograms;
  g_browser_process->local_state()->SetFilePath(
      prefs::kSodaBinaryPath,
      test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
          .Append(kSodaBinaryRelativePath));
  g_browser_process->local_state()->SetFilePath(
      prefs::kSodaEnUsConfigPath,
      test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
          .Append(kSodaLanguagePackRelativePath));

  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  profile_prefs->SetBoolean(prefs::kLiveCaptionEnabled, true);
  LaunchService();

  std::string buffer;
  auto audio_file = test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
                        .Append(base::FilePath(kSodaTestAudioRelativePath));
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::PathExists(audio_file));
    ASSERT_TRUE(base::ReadFileToString(audio_file, &buffer));
  }

  auto handler = media::WavAudioHandler::Create(buffer);
  ASSERT_TRUE(handler.get());
  ASSERT_EQ(handler->num_channels(), kExpectedChannelCount);

  auto bus =
      media::AudioBus::Create(kExpectedChannelCount, handler->total_frames());

  size_t bytes_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), 0, &bytes_written));

  std::vector<int16_t> audio_data(bus->frames());
  bus->ToInterleaved<media::SignedInt16SampleTypeTraits>(bus->frames(),
                                                         audio_data.data());

  constexpr size_t kMaxChunkSize = 1024;
  constexpr int kReplayAudioCount = 2;
  for (int i = 0; i < kReplayAudioCount; i++) {
    int chunk_start = 0;
    // Upload chunks of 1024 frames at a time.
    while (chunk_start < static_cast<int>(audio_data.size())) {
      int chunk_size = kMaxChunkSize < audio_data.size() - chunk_start
                           ? kMaxChunkSize
                           : audio_data.size() - chunk_start;

      auto signed_buffer = media::mojom::AudioDataS16::New();
      signed_buffer->channel_count = kExpectedChannelCount;
      signed_buffer->frame_count = chunk_size;
      signed_buffer->sample_rate = handler->sample_rate();
      for (int i = 0; i < chunk_size; i++) {
        signed_buffer->data.push_back(audio_data[chunk_start + i]);
      }

      speech_recognition_recognizer_->SendAudioToSpeechRecognitionService(
          std::move(signed_buffer));
      chunk_start += chunk_size;

      // Sleep for 20ms to simulate real-time audio. SODA requires audio
      // streaming in order to return events.
      usleep(20000);
    }

    speech_recognition_recognizer_->OnCaptionBubbleClosed();
  }

  speech_recognition_recognizer_.reset();
  base::RunLoop().RunUntilIdle();

  // Sleep for 50ms to ensure SODA has returned real-time results.
  usleep(50000);
  ASSERT_GT(static_cast<int>(recognition_results_.size()), kReplayAudioCount);
  ASSERT_EQ(recognition_results_.back(), "Hey Google Hey Google");

  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectUniqueTimeSample(
      SpeechRecognitionRecognizerImpl::kCaptionBubbleVisibleHistogramName,
      base::TimeDelta::FromMilliseconds(1260), 1);
  histograms.ExpectUniqueTimeSample(
      SpeechRecognitionRecognizerImpl::kCaptionBubbleHiddenHistogramName,
      base::TimeDelta::FromMilliseconds(1260), 1);
}

IN_PROC_BROWSER_TEST_F(SpeechRecognitionServiceTest, CreateAudioSourceFetcher) {
  base::HistogramTester histograms;
  g_browser_process->local_state()->SetFilePath(
      prefs::kSodaBinaryPath,
      test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
          .Append(kSodaBinaryRelativePath));
  g_browser_process->local_state()->SetFilePath(
      prefs::kSodaEnUsConfigPath,
      test_data_dir_.Append(base::FilePath(kSodaResourcesDir))
          .Append(kSodaLanguagePackRelativePath));

  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  // TODO(crbug.com/1173135): Disconnect from kLiveCaptionEnabled.
  profile_prefs->SetBoolean(prefs::kLiveCaptionEnabled, true);
  LaunchServiceWithAudioSourceFetcher();

  // TODO(crbug.com/1185978): Check implementation / sandbox policy on Mac and
  // Windows.
#if defined(OS_CHROMEOS) || defined(OS_LINUX)
  // Check that Start begins audio recording.
  // TODO(crbug.com/1173135): Try to mock audio input, maybe with
  // TestStreamFactory::stream_, to test end-to-end.
  std::string device_id = media::AudioDeviceDescription::kDefaultDeviceId;
  media::AudioParameters params(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                media::CHANNEL_LAYOUT_STEREO, 10000, 1000);

  // Create a fake stream factory.
  std::unique_ptr<StrictMock<TestStreamFactory>> stream_factory =
      std::make_unique<StrictMock<TestStreamFactory>>();
  EXPECT_CALL(stream_factory->stream_, Record());
  audio_source_fetcher_->Start(stream_factory->MakeRemote(), device_id, params);
  stream_factory->WaitToCreateInputStream();

  EXPECT_EQ(device_id, stream_factory->device_id_);
  ASSERT_TRUE(stream_factory->params_);
  EXPECT_TRUE(params.Equals(stream_factory->params_.value()));
#endif

  audio_source_fetcher_->Stop();
  base::RunLoop().RunUntilIdle();
}

}  // namespace speech
