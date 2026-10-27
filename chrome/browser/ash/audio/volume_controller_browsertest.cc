// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "content/public/browser/audio_service.h"
#include "content/public/test/browser_test.h"
#include "services/audio/public/cpp/sounds/global_sounds_manager.h"
#include "services/audio/public/mojom/audio_service.mojom.h"
#include "services/audio/test/fake_audio_service.h"
#include "services/audio/test/fake_output_stream.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/test/event_generator.h"

namespace {

using ::ash::AccessibilityManager;

class VolumeControllerTest : public InProcessBrowserTest {
 public:
  VolumeControllerTest() = default;

  VolumeControllerTest(const VolumeControllerTest&) = delete;
  VolumeControllerTest& operator=(const VolumeControllerTest&) = delete;

  ~VolumeControllerTest() override = default;

  void SetUpOnMainThread() override {
    audio_handler_ = ash::CrasAudioHandler::Get();
  }

  void VolumeUp() {
    ui::test::EventGenerator(ash::Shell::GetPrimaryRootWindow())
        .PressKey(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  }

  void VolumeDown() {
    ui::test::EventGenerator(ash::Shell::GetPrimaryRootWindow())
        .PressKey(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  }

  void VolumeMute() {
    ui::test::EventGenerator(ash::Shell::GetPrimaryRootWindow())
        .PressKey(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  }

 protected:
  raw_ptr<ash::CrasAudioHandler, DanglingUntriaged>
      audio_handler_;  // Not owned.
};

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpAndDown) {
  // Set initial value as 50%
  const int kInitVolume = 50;
  audio_handler_->SetOutputVolumePercent(kInitVolume);
  int current_volume = audio_handler_->GetOutputVolumePercent();
  EXPECT_EQ(current_volume, kInitVolume);

  current_volume = audio_handler_->GetOutputVolumePercent();
  VolumeUp();
  // number_of_volume_step = 25 mean we split volume into 25 level,
  // and The volume goes up one level each for VolumeUp/VolumeDown event.
  // For initial value is 48 - 51 volume will increase to 52,
  // because 48 - 51 share same level,
  // VolumeUp will increase to the min volume of next level, which is 52
  // Original behavior will set volume to 54
  EXPECT_LT(current_volume, audio_handler_->GetOutputVolumePercent());

  current_volume = audio_handler_->GetOutputVolumePercent();
  VolumeDown();
  // VolumeUp will decrease to the min volume of previous level, which is 48
  // Original behavior will set volume to 50
  EXPECT_GT(current_volume, audio_handler_->GetOutputVolumePercent());

  current_volume = audio_handler_->GetOutputVolumePercent();
  VolumeDown();
  EXPECT_GT(current_volume, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeDownToZero) {
  // Setting to very small volume.
  audio_handler_->SetOutputVolumePercent(1);

  VolumeDown();
  EXPECT_EQ(0, audio_handler_->GetOutputVolumePercent());
  VolumeDown();
  EXPECT_EQ(0, audio_handler_->GetOutputVolumePercent());
  VolumeUp();
  EXPECT_LT(0, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, VolumeUpTo100) {
  // Setting to almost max
  audio_handler_->SetOutputVolumePercent(99);

  VolumeUp();
  EXPECT_EQ(100, audio_handler_->GetOutputVolumePercent());
  VolumeUp();
  EXPECT_EQ(100, audio_handler_->GetOutputVolumePercent());
  VolumeDown();
  EXPECT_GT(100, audio_handler_->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerTest, Mutes) {
  ASSERT_FALSE(audio_handler_->IsOutputMuted());
  const int initial_volume = audio_handler_->GetOutputVolumePercent();

  VolumeMute();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());

  // Further mute buttons doesn't have effects.
  VolumeMute();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());

  // Right after the volume up after set_mute recovers to original volume.
  // Press volume up key will increase the volume from the original volume.
  VolumeUp();
  EXPECT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_LT(initial_volume, audio_handler_->GetOutputVolumePercent());

  VolumeMute();
  // After the volume down, press volume down key will decrease the volume from
  // the original volume while the volume is still muted.
  VolumeDown();
  EXPECT_TRUE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(initial_volume, audio_handler_->GetOutputVolumePercent());

  // Thus, further VolumeUp will increase the volume.
  VolumeUp();
  EXPECT_LT(initial_volume, audio_handler_->GetOutputVolumePercent());
}

class VolumeControllerSoundsTest : public VolumeControllerTest {
 public:
  VolumeControllerSoundsTest() = default;

  VolumeControllerSoundsTest(const VolumeControllerSoundsTest&) = delete;
  VolumeControllerSoundsTest& operator=(const VolumeControllerSoundsTest&) =
      delete;

  ~VolumeControllerSoundsTest() override = default;

  void SetUpOnMainThread() override {
    VolumeControllerTest::SetUpOnMainThread();
    fake_audio_service_ = std::make_unique<audio::FakeAudioService>();
    audio_service_override_.emplace(
        content::OverrideAudioServiceForTesting(fake_audio_service_.get()));
  }

  void TearDownOnMainThread() override {
    audio_service_override_.reset();
    fake_audio_service_.reset();
    VolumeControllerTest::TearDownOnMainThread();
  }

  bool is_sound_initialized() const {
    return !audio::GlobalSoundsManager::Get()
                .GetDuration(static_cast<int>(ash::Sound::kVolumeAdjust))
                .is_zero();
  }

  // We must explicitly stop the sound to force the stream to close.
  // In production, the stream remains alive for 1500ms after playback finishes
  // to allow reuse. In tests, we want to verify each play request creates a new
  // stream (to assert on play counts) without waiting 1500ms of real time.
  // Additionally, because browser tests do not drive the audio clock, the
  // stream would otherwise get stuck in the active state forever.
  void FinishSound(ash::Sound sound) {
    audio::GlobalSoundsManager::Get().Stop(static_cast<int>(sound));
    fake_audio_service_->fake_output_stream().ExpectDisconnect();
  }

 protected:
  std::unique_ptr<audio::FakeAudioService> fake_audio_service_;
  std::optional<base::AutoReset<audio::mojom::AudioService*>>
      audio_service_override_;
};

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsTest, Simple) {
  auto& fake_output_stream = fake_audio_service_->fake_output_stream();
  EXPECT_TRUE(is_sound_initialized());
  audio_handler_->SetOutputVolumePercent(50);

  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  VolumeUp();
  VolumeDown();
  EXPECT_EQ(0, fake_output_stream.play_count());

  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(1, fake_output_stream.play_count());
  FinishSound(ash::Sound::kSpokenFeedbackEnabled);

  VolumeUp();
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(2, fake_output_stream.play_count());
  FinishSound(ash::Sound::kVolumeAdjust);

  VolumeDown();
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(3, fake_output_stream.play_count());
  FinishSound(ash::Sound::kVolumeAdjust);
}

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsTest, EdgeCases) {
  auto& fake_output_stream = fake_audio_service_->fake_output_stream();
  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(1, fake_output_stream.play_count());
  FinishSound(ash::Sound::kSpokenFeedbackEnabled);

  // Check that sound is played on volume up and volume down.
  audio_handler_->SetOutputVolumePercent(50);
  VolumeUp();
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(2, fake_output_stream.play_count());
  FinishSound(ash::Sound::kVolumeAdjust);

  VolumeDown();
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(3, fake_output_stream.play_count());
  FinishSound(ash::Sound::kVolumeAdjust);

  audio_handler_->SetOutputVolumePercent(99);
  VolumeUp();
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(4, fake_output_stream.play_count());
  FinishSound(ash::Sound::kVolumeAdjust);

  audio_handler_->SetOutputVolumePercent(100);
  VolumeUp();
  EXPECT_EQ(4, fake_output_stream.play_count());

  // Check that sound isn't played when audio is muted.
  audio_handler_->SetOutputVolumePercent(50);
  VolumeMute();
  VolumeDown();
  ASSERT_TRUE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(4, fake_output_stream.play_count());

  // Check that audio is unmuted and sound is played.
  VolumeUp();
  ASSERT_FALSE(audio_handler_->IsOutputMuted());
  fake_output_stream.ExpectPlay();
  EXPECT_EQ(5, fake_output_stream.play_count());
  FinishSound(ash::Sound::kVolumeAdjust);
}

class VolumeControllerSoundsDisabledTest : public VolumeControllerSoundsTest {
 public:
  VolumeControllerSoundsDisabledTest() = default;

  VolumeControllerSoundsDisabledTest(
      const VolumeControllerSoundsDisabledTest&) = delete;
  VolumeControllerSoundsDisabledTest& operator=(
      const VolumeControllerSoundsDisabledTest&) = delete;

  ~VolumeControllerSoundsDisabledTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    VolumeControllerSoundsTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kDisableVolumeAdjustSound);
  }
};

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsDisabledTest, VolumeAdjustSounds) {
  auto& fake_output_stream = fake_audio_service_->fake_output_stream();

  // Check that sound isn't played on volume up and volume down.
  audio_handler_->SetOutputVolumePercent(50);
  VolumeUp();
  VolumeDown();
  EXPECT_EQ(0, fake_output_stream.play_count());
}

}  // namespace
