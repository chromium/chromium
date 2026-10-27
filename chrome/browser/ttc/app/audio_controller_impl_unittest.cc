// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/audio_controller_impl.h"

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/ttc/app/public/error_codes.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

namespace {

class MockAudioControllerObserver : public AudioController::Observer {
 public:
  MOCK_METHOD(void,
              OnCapturedAudio,
              (base::span<const int16_t> pcm_data,
               const media::AudioParameters& params,
               float energy),
              (override));
  MOCK_METHOD(void, OnCaptureStopped, (), (override));
  MOCK_METHOD(void,
              OnModelAudioEnvelopeReady,
              (const AudioEnvelope& envelope),
              (override));
  MOCK_METHOD(void, OnPlaybackCompleted, (int64_t sequence_number), (override));
  MOCK_METHOD(void, OnAudioError, (ErrorCode error), (override));
};

}  // namespace

class AudioControllerImplTest : public testing::Test {
 public:
  AudioControllerImplTest() {
    audio_manager_.SetHasInputDevices(true);
    audio_manager_.SetInputStreamParameters(
        media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               media::ChannelLayoutConfig::Stereo(),
                               /*sample_rate=*/44100,
                               /*frames_per_buffer=*/441));
  }

  ~AudioControllerImplTest() override { audio_manager_.Shutdown(); }

 protected:
  // Returns a factory handing out AudioSystems backed by `audio_manager_`.
  AudioControllerImpl::AudioSystemFactory GetAudioSystemFactory() {
    return base::BindLambdaForTesting(
        [this]() -> std::unique_ptr<media::AudioSystem> {
          return std::make_unique<media::AudioSystemImpl>(&audio_manager_);
        });
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  media::MockAudioManager audio_manager_{
      std::make_unique<media::TestAudioThread>()};
  // Outlives the AudioControllerImpl created by each test.
  testing::NiceMock<MockAudioControllerObserver> observer_;
};

TEST_F(AudioControllerImplTest, LoopbackCaptureToPlayback) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  std::vector<int16_t> captured_pcm;
  base::RunLoop capture_loop;
  EXPECT_CALL(observer_, OnCapturedAudio)
      .WillOnce([&](base::span<const int16_t> pcm_data,
                    const media::AudioParameters& params, float energy) {
        captured_pcm.assign(pcm_data.begin(), pcm_data.end());
        // Feed mic input directly into playback (loopback)
        controller.PlayAudio(pcm_data, params, /*sequence_number=*/101);
        capture_loop.Quit();
      });

  int64_t completed_seq = -1;
  base::RunLoop completion_loop;
  EXPECT_CALL(observer_, OnPlaybackCompleted)
      .WillOnce([&](int64_t sequence_number) {
        completed_seq = sequence_number;
        completion_loop.Quit();
      });

  const int frames = 1600;
  auto input_bus = media::AudioBus::Create(1, frames);
  for (int i = 0; i < frames; ++i) {
    // Generate a simple linear ramp signal normalized between -0.5 and +0.5
    input_bus->channel(0)[i] = (static_cast<float>(i) / frames) - 0.5f;
  }

  // Simulate microphone capture
  controller.Capture(input_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  capture_loop.Run();

  EXPECT_FALSE(captured_pcm.empty());
  EXPECT_EQ(captured_pcm.size(), static_cast<size_t>(frames));
  EXPECT_TRUE(controller.is_playing());

  // Simulate speaker render
  auto output_bus = media::AudioBus::Create(1, frames);
  output_bus->Zero();
  int rendered = controller.Render(base::TimeDelta(), base::TimeTicks::Now(),
                                   {}, output_bus.get());
  EXPECT_EQ(rendered, frames);
  completion_loop.Run();

  EXPECT_EQ(completed_seq, 101);
  EXPECT_FALSE(controller.is_playing());

  // Verify the rendered audio samples match the input audio samples closely
  for (int i = 0; i < frames; ++i) {
    EXPECT_NEAR(output_bus->channel(0)[i], input_bus->channel(0)[i], 0.001f);
  }
}

TEST_F(AudioControllerImplTest, LoopbackWithDelay) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  base::RunLoop capture_loop;
  EXPECT_CALL(observer_, OnCapturedAudio)
      .WillOnce([&](base::span<const int16_t> pcm_data,
                    const media::AudioParameters& params, float energy) {
        std::vector<int16_t> pcm_copy(pcm_data.begin(), pcm_data.end());
        // Delayed loopback after 50ms
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting(
                [&controller, pcm_copy = std::move(pcm_copy), params]() {
                  controller.PlayAudio(pcm_copy, params,
                                       /*sequence_number=*/202);
                }),
            base::Milliseconds(50));
        capture_loop.Quit();
      });

  const int frames = 1600;
  auto input_bus = media::AudioBus::Create(1, frames);
  for (int i = 0; i < frames; ++i) {
    input_bus->channel(0)[i] = 0.25f;
  }

  controller.Capture(input_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  capture_loop.Run();

  // Initially before delay expires, nothing is queued for playback yet
  EXPECT_FALSE(controller.is_playing());

  // Fast-forward time to trigger delayed playback
  task_environment_.FastForwardBy(base::Milliseconds(50));
  EXPECT_TRUE(controller.is_playing());

  auto output_bus = media::AudioBus::Create(1, frames);
  controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                    output_bus.get());

  for (int i = 0; i < frames; ++i) {
    EXPECT_NEAR(output_bus->channel(0)[i], 0.25f, 0.001f);
  }
}

TEST_F(AudioControllerImplTest, AudioEnergyCalculation) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  float last_energy = -1.0f;
  base::RunLoop energy_loop;
  EXPECT_CALL(observer_, OnCapturedAudio)
      .WillOnce([&](base::span<const int16_t> pcm_data,
                    const media::AudioParameters& params, float energy) {
        last_energy = energy;
        energy_loop.Quit();
      });

  const int frames = 1600;
  auto input_bus = media::AudioBus::Create(1, frames);
  for (int i = 0; i < frames; ++i) {
    input_bus->channel(0)[i] = 0.5f;
  }

  controller.Capture(input_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  energy_loop.Run();

  EXPECT_NEAR(last_energy, 0.5f, 0.01f);
}

TEST_F(AudioControllerImplTest, PlaybackQueueClearing) {
  AudioControllerImpl controller;

  std::vector<int16_t> pcm_chunk(1600, 0x3030);
  controller.PlayAudio(pcm_chunk, /*sequence_number=*/1);
  EXPECT_TRUE(controller.is_playing());

  controller.ClearPlaybackQueue();
  EXPECT_FALSE(controller.is_playing());

  auto output_bus = media::AudioBus::Create(1, 1600);
  output_bus->Zero();
  int rendered = controller.Render(base::TimeDelta(), base::TimeTicks::Now(),
                                   {}, output_bus.get());
  EXPECT_EQ(rendered, 1600);
  // All output should be zeros
  for (int i = 0; i < 1600; ++i) {
    EXPECT_EQ(output_bus->channel(0)[i], 0.0f);
  }
}

TEST_F(AudioControllerImplTest, PartialFrameRendering) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  // 1000 frames of PCM16 mono (2000 bytes)
  std::vector<int16_t> samples(1000, 10000);

  int64_t completed_seq = -1;
  base::RunLoop completion_loop;
  EXPECT_CALL(observer_, OnPlaybackCompleted).WillOnce([&](int64_t seq) {
    completed_seq = seq;
    completion_loop.Quit();
  });

  controller.PlayAudio(samples, /*sequence_number=*/456);

  // Render first 400 frames
  auto bus1 = media::AudioBus::Create(1, 400);
  controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {}, bus1.get());
  EXPECT_EQ(completed_seq, -1);
  EXPECT_TRUE(controller.is_playing());

  // Render second 400 frames
  auto bus2 = media::AudioBus::Create(1, 400);
  controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {}, bus2.get());
  EXPECT_EQ(completed_seq, -1);
  EXPECT_TRUE(controller.is_playing());

  // Render remaining 400 frames (chunk only has 200 frames left)
  auto bus3 = media::AudioBus::Create(1, 400);
  controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {}, bus3.get());
  completion_loop.Run();
  EXPECT_EQ(completed_seq, 456);
  EXPECT_FALSE(controller.is_playing());

  // First 200 frames of bus3 should be non-zero, next 200 frames should be zero
  for (int i = 0; i < 200; ++i) {
    EXPECT_GT(bus3->channel(0)[i], 0.0f);
  }
  for (int i = 200; i < 400; ++i) {
    EXPECT_EQ(bus3->channel(0)[i], 0.0f);
  }
}

TEST_F(AudioControllerImplTest,
       RenderMultiChannelDuplicatesMonoToAllChannels) {
  AudioControllerImpl controller;

  std::vector<int16_t> samples(200, 15000);
  controller.PlayAudio(samples, /*sequence_number=*/1);

  // Render into stereo (2 channels)
  auto stereo_bus = media::AudioBus::Create(2, 200);
  int frames = controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {},
                                 stereo_bus.get());
  EXPECT_EQ(frames, 200);

  for (int i = 0; i < 200; ++i) {
    EXPECT_GT(stereo_bus->channel(0)[i], 0.0f);
    EXPECT_EQ(stereo_bus->channel(0)[i], stereo_bus->channel(1)[i]);
  }
}

TEST_F(AudioControllerImplTest, RenderNullBusReturnsZero) {
  AudioControllerImpl controller;
  EXPECT_EQ(
      controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {}, nullptr),
      0);
}

TEST_F(AudioControllerImplTest, PlayEmptyAudioChunkDoesNotQueue) {
  AudioControllerImpl controller;
  controller.PlayAudio(base::span<const int16_t>(), /*sequence_number=*/1);
  EXPECT_FALSE(controller.is_playing());
}

TEST_F(AudioControllerImplTest, PlayPlaceholderAudioChunkDoesNotQueue) {
  AudioControllerImpl controller;
  // A single sample is treated as a placeholder chunk and dropped.
  std::vector<int16_t> tiny_chunk(1, 0);
  controller.PlayAudio(tiny_chunk, /*sequence_number=*/1);
  EXPECT_FALSE(controller.is_playing());
}

TEST_F(AudioControllerImplTest, StartAndStopCaptureWithFakeBinder) {
  bool binder_called = false;
  auto fake_binder = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
        binder_called = true;
      });

  AudioControllerImpl controller(fake_binder, GetAudioSystemFactory());
  EXPECT_FALSE(controller.is_capturing());

  controller.StartCapture(/*device_id=*/"");
  EXPECT_TRUE(controller.is_capturing());

  // The stream is only created once the device parameters have been received.
  EXPECT_FALSE(binder_called);
  EXPECT_TRUE(base::test::RunUntil([&]() { return binder_called; }));
  EXPECT_TRUE(controller.is_capturing());

  // Redundant StartCapture is a no-op
  controller.StartCapture(/*device_id=*/"");
  EXPECT_TRUE(controller.is_capturing());

  controller.StopCapture();
  EXPECT_FALSE(controller.is_capturing());
}

TEST_F(AudioControllerImplTest, StopCaptureNotifiesObservers) {
  AudioControllerImpl controller(base::DoNothing(), GetAudioSystemFactory());
  controller.AddObserver(&observer_);
  controller.StartCapture(/*device_id=*/"");

  EXPECT_CALL(observer_, OnCaptureStopped).Times(1);
  controller.StopCapture();
  // Already stopped.
  controller.StopCapture();
}

TEST_F(AudioControllerImplTest, StopCaptureDropsUndeliveredAudio) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  // A full chunk is captured and posted to the main thread, but capture stops
  // before it's delivered.
  auto bus = media::AudioBus::Create(1, 1600);
  bus->Zero();
  controller.Capture(bus.get(), base::TimeTicks::Now(), {}, 1.0);
  controller.StopCapture();

  EXPECT_CALL(observer_, OnCapturedAudio).Times(0);
  // Runs the tasks posted so far, including the one delivering the chunk.
  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(AudioControllerImplTest,
       StartCaptureWithoutInputDevicesReportsError) {
  audio_manager_.SetHasInputDevices(false);

  bool binder_called = false;
  auto fake_binder = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
        binder_called = true;
      });
  AudioControllerImpl controller(fake_binder, GetAudioSystemFactory());
  controller.AddObserver(&observer_);

  std::vector<ErrorCode> errors;
  base::RunLoop error_loop;
  EXPECT_CALL(observer_, OnAudioError).WillOnce([&](ErrorCode error) {
    errors.push_back(error);
    error_loop.Quit();
  });

  controller.StartCapture(/*device_id=*/"");
  error_loop.Run();

  EXPECT_THAT(errors,
              testing::ElementsAre(ErrorCode::kAudioNoMicrophoneDetected));
  EXPECT_FALSE(controller.is_capturing());
  EXPECT_FALSE(binder_called);
}

TEST_F(AudioControllerImplTest, CaptureErrorsAreConvertedAndPostedToObservers) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  std::vector<ErrorCode> errors;
  EXPECT_CALL(observer_, OnAudioError).WillRepeatedly([&](ErrorCode error) {
    errors.push_back(error);
  });

  const struct {
    media::AudioCapturerSource::ErrorCode capture_error;
    ErrorCode expected_error;
  } kTestCases[] = {
      {media::AudioCapturerSource::ErrorCode::kSystemPermissions,
       ErrorCode::kAudioNoMicrophoneDetected},
      {media::AudioCapturerSource::ErrorCode::kDeviceRemoved,
       ErrorCode::kAudioNoMicrophoneDetected},
      {media::AudioCapturerSource::ErrorCode::kDeviceInUse,
       ErrorCode::kAudioMicrophoneInUse},
      {media::AudioCapturerSource::ErrorCode::kUnknown,
       ErrorCode::kAudioUnknownError},
      {media::AudioCapturerSource::ErrorCode::kSocketError,
       ErrorCode::kAudioUnknownError},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(static_cast<int>(test_case.capture_error));
    errors.clear();

    controller.OnCaptureError(test_case.capture_error, "error");
    EXPECT_TRUE(errors.empty());

    ASSERT_TRUE(base::test::RunUntil([&] { return !errors.empty(); }));
    EXPECT_THAT(errors, testing::ElementsAre(test_case.expected_error));
  }
}

TEST_F(AudioControllerImplTest,
       CaptureErrorIsDroppedIfControllerIsDestroyed) {
  auto controller = std::make_unique<AudioControllerImpl>();
  controller->AddObserver(&observer_);

  EXPECT_CALL(observer_, OnAudioError).Times(0);

  controller->OnCaptureError(media::AudioCapturerSource::ErrorCode::kUnknown,
                             "error");
  controller.reset();

  base::RunLoop run_loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

class FakeStreamFactoryForAec : public media::mojom::AudioStreamFactory {
 public:
  FakeStreamFactoryForAec() = default;
  ~FakeStreamFactoryForAec() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void CreateInputStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      uint32_t shared_memory_count,
      bool enable_agc,
      media::mojom::AudioProcessingConfigPtr processing_config,
      CreateInputStreamCallback created_callback) override {
    stream_created_ = true;
    last_device_id_ = device_id;
    last_params_ = params;
    last_enable_agc_ = enable_agc;
    has_processing_config_ = !processing_config.is_null();
    if (processing_config) {
      last_processing_settings_ = processing_config->settings;
    }
    std::move(created_callback)
        .Run(nullptr, /*initially_muted=*/false, std::nullopt);
  }

  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) override {
    last_aec_output_device_id_ = output_device_id;
  }

  void CreateOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) override {}

  void CreateSwitchableOutputStream(
      mojo::PendingReceiver<media::mojom::AudioOutputStream> stream_receiver,
      mojo::PendingReceiver<media::mojom::DeviceSwitchInterface>
          device_switch_receiver,
      mojo::PendingAssociatedRemote<media::mojom::AudioOutputStreamObserver>
          observer,
      mojo::PendingRemote<media::mojom::AudioLog> log,
      const std::string& output_device_id,
      const media::AudioParameters& params,
      const base::UnguessableToken& group_id,
      CreateOutputStreamCallback created_callback) override {}

  void BindMuter(
      mojo::PendingAssociatedReceiver<media::mojom::LocalMuter> receiver,
      const base::UnguessableToken& group_id) override {}

  void CreateLoopbackStream(
      mojo::PendingReceiver<media::mojom::AudioInputStream> receiver,
      mojo::PendingRemote<media::mojom::AudioInputStreamClient> client,
      mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer,
      const media::AudioParameters& params,
      uint32_t shared_memory_count,
      const base::UnguessableToken& group_id,
      CreateLoopbackStreamCallback created_callback) override {}

  bool stream_created() const { return stream_created_; }
  const std::string& last_device_id() const { return last_device_id_; }
  const media::AudioParameters& last_params() const { return last_params_; }
  bool last_enable_agc() const { return last_enable_agc_; }
  bool has_processing_config() const { return has_processing_config_; }
  const std::optional<media::AudioProcessingSettings>&
  last_processing_settings() const {
    return last_processing_settings_;
  }
  const std::string& last_aec_output_device_id() const {
    return last_aec_output_device_id_;
  }

 private:
  mojo::Receiver<media::mojom::AudioStreamFactory> receiver_{this};
  std::string last_device_id_;
  media::AudioParameters last_params_;
  bool stream_created_ = false;
  bool last_enable_agc_ = false;
  bool has_processing_config_ = false;
  std::optional<media::AudioProcessingSettings> last_processing_settings_;
  std::string last_aec_output_device_id_;
};

TEST_F(AudioControllerImplTest, HardwareAecDeviceDoesNotEngageSoftwareAec) {
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kChromeWideEchoCancellation);

  // Configure hardware device parameters with ECHO_CANCELLER effect only.
  media::AudioParameters hw_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  hw_params.set_effects(media::AudioParameters::ECHO_CANCELLER);
  audio_manager_.SetInputStreamParameters(hw_params);

  FakeStreamFactoryForAec fake_factory;
  auto fake_binder = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
        fake_factory.Bind(std::move(receiver));
      });

  AudioControllerImpl controller(fake_binder, GetAudioSystemFactory());
  controller.StartCapture(/*device_id=*/"");
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_factory.last_params().sample_rate() > 0; }));

  // Hardware AEC preferred, but software NS and AGC should still be enabled.
  EXPECT_TRUE(fake_factory.has_processing_config());
  ASSERT_TRUE(fake_factory.last_processing_settings().has_value());
  EXPECT_FALSE(fake_factory.last_processing_settings()->echo_cancellation);
  EXPECT_TRUE(fake_factory.last_processing_settings()->noise_suppression);
  EXPECT_TRUE(fake_factory.last_processing_settings()->automatic_gain_control);
  EXPECT_TRUE(fake_factory.last_enable_agc());
  EXPECT_EQ(fake_factory.last_params().sample_rate(), 16000);
  EXPECT_EQ(fake_factory.last_params().frames_per_buffer(), 160);
#endif
}

TEST_F(AudioControllerImplTest,
       HardwareSupportingAllEffectsDoesNotEngageSoftwareProcessing) {
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kChromeWideEchoCancellation);
#endif

  // Configure hardware device parameters with all effects supported in HW.
  media::AudioParameters hw_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 48000, 480);
  hw_params.set_effects(media::AudioParameters::ECHO_CANCELLER |
                        media::AudioParameters::NOISE_SUPPRESSION |
                        media::AudioParameters::AUTOMATIC_GAIN_CONTROL);
  audio_manager_.SetInputStreamParameters(hw_params);

  FakeStreamFactoryForAec fake_factory;
  auto fake_binder = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
        fake_factory.Bind(std::move(receiver));
      });

  AudioControllerImpl controller(fake_binder, GetAudioSystemFactory());
  controller.StartCapture(/*device_id=*/"");
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_factory.last_params().sample_rate() > 0; }));

  // All effects handled by hardware: software processing config should NOT be
  // sent, and the input device should be opened with native sample rate.
  EXPECT_FALSE(fake_factory.has_processing_config());
  EXPECT_FALSE(fake_factory.last_enable_agc());
  EXPECT_EQ(fake_factory.last_params().sample_rate(), 48000);
}

TEST_F(AudioControllerImplTest, HardwareLackingAecEngagesSoftwareAec) {
#if BUILDFLAG(CHROME_WIDE_ECHO_CANCELLATION)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(media::kChromeWideEchoCancellation);

  // Configure device parameters WITHOUT ECHO_CANCELLER effect.
  media::AudioParameters no_hw_aec_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), 44100, 441);
  audio_manager_.SetInputStreamParameters(no_hw_aec_params);

  FakeStreamFactoryForAec fake_factory;
  auto fake_binder = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
        fake_factory.Bind(std::move(receiver));
      });

  AudioControllerImpl controller(fake_binder, GetAudioSystemFactory());
  controller.StartCapture(/*device_id=*/"");
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_factory.last_params().sample_rate() > 0; }));

  // Software fallback: WebRTC processing config must be sent.
  EXPECT_TRUE(fake_factory.has_processing_config());
  ASSERT_TRUE(fake_factory.last_processing_settings().has_value());
  EXPECT_TRUE(fake_factory.last_processing_settings()->echo_cancellation);
  EXPECT_TRUE(fake_factory.last_processing_settings()->noise_suppression);
  EXPECT_TRUE(fake_factory.last_processing_settings()->automatic_gain_control);
  EXPECT_TRUE(fake_factory.last_enable_agc());

  // Audio service delivers 10ms (160 frames) at 16kHz mono.
  EXPECT_EQ(fake_factory.last_params().sample_rate(), 16000);
  EXPECT_EQ(fake_factory.last_params().frames_per_buffer(), 160);
#endif
}

TEST_F(AudioControllerImplTest,
       FifoAggregatesTen10msChunksIntoOne100msChunk) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  int callback_count = 0;
  std::vector<int16_t> delivered_pcm;
  base::RunLoop capture_loop;
  EXPECT_CALL(observer_, OnCapturedAudio)
      .WillRepeatedly([&](base::span<const int16_t> pcm_data,
                          const media::AudioParameters& params, float energy) {
        callback_count++;
        delivered_pcm.assign(pcm_data.begin(), pcm_data.end());
        EXPECT_EQ(params.sample_rate(), 16000);
        EXPECT_EQ(params.frames_per_buffer(), 1600);
        capture_loop.Quit();
      });

  // Deliver nine 10ms chunks (160 frames each at 16kHz) -> total 1440 frames.
  // Should NOT trigger delivery yet (1600 required).
  auto chunk_bus = media::AudioBus::Create(1, 160);
  chunk_bus->Zero();
  for (int i = 0; i < 9; ++i) {
    controller.Capture(chunk_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  }
  EXPECT_EQ(callback_count, 0);

  // Deliver the 10th 10ms chunk (total reaches 1600 frames).
  // Exactly one 100ms chunk (1600 frames) should be emitted.
  controller.Capture(chunk_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  capture_loop.Run();

  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(delivered_pcm.size(), 1600u);
}

TEST_F(AudioControllerImplTest, StopCaptureFlushesFifo) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  int callback_count = 0;
  EXPECT_CALL(observer_, OnCapturedAudio)
      .WillRepeatedly([&](base::span<const int16_t> pcm_data,
                          const media::AudioParameters& params,
                          float energy) { callback_count++; });

  // Push partial audio into FIFO (5 chunks of 160 frames = 800 frames).
  auto chunk_bus = media::AudioBus::Create(1, 160);
  chunk_bus->Zero();
  for (int i = 0; i < 5; ++i) {
    controller.Capture(chunk_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  }
  EXPECT_EQ(callback_count, 0);

  // StopCapture resets the FIFO.
  controller.StopCapture();

  // In a new session, pushing 5 more chunks (800 frames) should NOT trigger
  // delivery from old leftover frames (it would if old 800 + new 800 = 1600).
  for (int i = 0; i < 5; ++i) {
    controller.Capture(chunk_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  }
  EXPECT_EQ(callback_count, 0);
}

TEST_F(AudioControllerImplTest, PlayAudioExtractsAndDispatchesAudioEnvelope) {
  AudioControllerImpl controller;
  controller.AddObserver(&observer_);

  // 120ms of 440Hz sine wave at 24kHz (2880 samples = 6 slices of 20ms)
  // followed by 40ms of silence (960 samples = 2 slices of 20ms).
  std::vector<int16_t> pcm_samples(2880 + 960, 0);
  for (size_t i = 0; i < 2880; ++i) {
    pcm_samples[i] = static_cast<int16_t>(
        16000.0 *
        std::sin(2.0 * std::numbers::pi_v<double> * 440.0 * i / 24000.0));
  }

  AudioEnvelope first_envelope;
  EXPECT_CALL(observer_, OnModelAudioEnvelopeReady(testing::_))
      .WillOnce(testing::SaveArg<0>(&first_envelope));
  controller.PlayAudio(pcm_samples, /*sequence_number=*/1);

  EXPECT_GT(first_envelope.rms, 0.3f);
  EXPECT_GT(first_envelope.peak_amplitude, 0.45f);
  EXPECT_GT(first_envelope.zero_crossing_rate, 0.0f);
  ASSERT_EQ(first_envelope.segments.size(), 2u);
  EXPECT_EQ(first_envelope.segments[0].start_time, base::Milliseconds(0));
  EXPECT_EQ(first_envelope.segments[0].duration, base::Milliseconds(120));
  EXPECT_EQ(first_envelope.segments[1].start_time, base::Milliseconds(120));
  EXPECT_EQ(first_envelope.segments[1].duration, base::Milliseconds(40));

  // A subsequent PlayAudio chunk continues cumulative segment offsets.
  AudioEnvelope second_envelope;
  EXPECT_CALL(observer_, OnModelAudioEnvelopeReady(testing::_))
      .WillOnce(testing::SaveArg<0>(&second_envelope));
  controller.PlayAudio(base::span(pcm_samples).first(480u),
                       /*sequence_number=*/2);
  ASSERT_EQ(second_envelope.segments.size(), 1u);
  EXPECT_EQ(second_envelope.segments[0].start_time, base::Milliseconds(160));
  EXPECT_EQ(second_envelope.segments[0].duration, base::Milliseconds(20));

  // Clearing the playback queue resets the sample offset back to 0ms.
  controller.ClearPlaybackQueue();
  AudioEnvelope third_envelope;
  EXPECT_CALL(observer_, OnModelAudioEnvelopeReady(testing::_))
      .WillOnce(testing::SaveArg<0>(&third_envelope));
  controller.PlayAudio(base::span(pcm_samples).first(480u),
                       /*sequence_number=*/3);
  ASSERT_EQ(third_envelope.segments.size(), 1u);
  EXPECT_EQ(third_envelope.segments[0].start_time, base::Milliseconds(0));
  EXPECT_EQ(third_envelope.segments[0].duration, base::Milliseconds(20));
}

}  // namespace ttc
