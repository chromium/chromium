// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/audio_controller.h"

#include <cmath>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {

class AudioControllerTest : public testing::Test {
 public:
  AudioControllerTest() = default;
  ~AudioControllerTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AudioControllerTest, LoopbackCaptureToPlayback) {
  AudioController controller;

  std::vector<uint8_t> captured_pcm;
  base::RunLoop capture_loop;
  auto capture_sub = controller.AddAudioCaptureListener(
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& pcm_data,
                                     const media::AudioParameters& params) {
        captured_pcm = pcm_data;
        // Feed mic input directly into playback (loopback)
        controller.PlayAudio(pcm_data, params, /*sequence_number=*/101);
        capture_loop.Quit();
      }));

  int64_t completed_seq = -1;
  base::RunLoop completion_loop;
  auto completion_sub = controller.AddPlaybackCompletionListener(
      base::BindLambdaForTesting([&](int64_t sequence_number) {
        completed_seq = sequence_number;
        completion_loop.Quit();
      }));

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
  EXPECT_EQ(captured_pcm.size(), static_cast<size_t>(frames * sizeof(int16_t)));
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

TEST_F(AudioControllerTest, LoopbackWithDelay) {
  AudioController controller;

  base::RunLoop capture_loop;
  auto capture_sub = controller.AddAudioCaptureListener(
      base::BindLambdaForTesting([&](const std::vector<uint8_t>& pcm_data,
                                     const media::AudioParameters& params) {
        // Delayed loopback after 50ms
        base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting([&controller, pcm_data, params]() {
              controller.PlayAudio(pcm_data, params, /*sequence_number=*/202);
            }),
            base::Milliseconds(50));
        capture_loop.Quit();
      }));

  const int frames = 800;
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

TEST_F(AudioControllerTest, AudioEnergyCalculation) {
  AudioController controller;

  float last_energy = -1.0f;
  base::RunLoop energy_loop;
  auto energy_sub = controller.AddAudioEnergyListener(
      base::BindLambdaForTesting([&](float energy) {
        last_energy = energy;
        if (energy > 0.0f) {
          energy_loop.Quit();
        }
      }));

  const int frames = 1600;
  auto input_bus = media::AudioBus::Create(1, frames);
  for (int i = 0; i < frames; ++i) {
    input_bus->channel(0)[i] = 0.5f;
  }

  controller.Capture(input_bus.get(), base::TimeTicks::Now(), {}, 1.0);
  energy_loop.Run();

  EXPECT_NEAR(last_energy, 0.5f, 0.01f);

  // Clear playback queue should reset energy to 0.0
  controller.ClearPlaybackQueue();
  EXPECT_EQ(last_energy, 0.0f);
}

TEST_F(AudioControllerTest, PlaybackQueueClearing) {
  AudioController controller;

  std::vector<uint8_t> pcm_chunk(3200, 0x30);
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

TEST_F(AudioControllerTest, PartialFrameRendering) {
  AudioController controller;

  // 1000 frames of PCM16 mono (2000 bytes)
  std::vector<int16_t> samples(1000, 10000);

  int64_t completed_seq = -1;
  base::RunLoop completion_loop;
  auto completion_sub = controller.AddPlaybackCompletionListener(
      base::BindLambdaForTesting([&](int64_t seq) {
        completed_seq = seq;
        completion_loop.Quit();
      }));

  controller.PlayAudio(base::as_byte_span(samples), /*sequence_number=*/456);

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

TEST_F(AudioControllerTest, RenderMultiChannelDuplicatesMonoToAllChannels) {
  AudioController controller;

  std::vector<int16_t> samples(200, 15000);
  controller.PlayAudio(base::as_byte_span(samples), /*sequence_number=*/1);

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

TEST_F(AudioControllerTest, RenderNullBusReturnsZero) {
  AudioController controller;
  EXPECT_EQ(
      controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {}, nullptr),
      0);
}

TEST_F(AudioControllerTest, PlayEmptyAudioChunkDoesNotQueue) {
  AudioController controller;
  controller.PlayAudio(base::span<const uint8_t>(), /*sequence_number=*/1);
  EXPECT_FALSE(controller.is_playing());
}

TEST_F(AudioControllerTest, PlaybackRendersTriggerEnergyListener) {
  AudioController controller;

  float last_energy = -1.0f;
  base::RunLoop energy_loop;
  auto energy_sub = controller.AddAudioEnergyListener(
      base::BindLambdaForTesting([&](float energy) {
        last_energy = energy;
        if (energy > 0.0f) {
          energy_loop.Quit();
        }
      }));

  std::vector<int16_t> samples(400, 16000);
  controller.PlayAudio(base::as_byte_span(samples), /*sequence_number=*/1);

  auto bus = media::AudioBus::Create(1, 400);
  controller.Render(base::TimeDelta(), base::TimeTicks::Now(), {}, bus.get());
  energy_loop.Run();

  EXPECT_GT(last_energy, 0.0f);
}

TEST_F(AudioControllerTest, StartAndStopCaptureWithFakeBinder) {
  bool binder_called = false;
  auto fake_binder = base::BindLambdaForTesting(
      [&](mojo::PendingReceiver<media::mojom::AudioStreamFactory> receiver) {
        binder_called = true;
      });

  AudioController controller(fake_binder);
  EXPECT_FALSE(controller.is_capturing());

  controller.StartCapture();
  EXPECT_TRUE(binder_called);
  EXPECT_TRUE(controller.is_capturing());

  // Redundant StartCapture is a no-op
  controller.StartCapture();
  EXPECT_TRUE(controller.is_capturing());

  controller.StopCapture();
  EXPECT_FALSE(controller.is_capturing());
}

}  // namespace ttc
