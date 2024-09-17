// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string_view>

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/audio/public/cpp/sounds/sounds_manager.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "content/public/test/browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/test/event_generator.h"

namespace {

using ::ash::AccessibilityManager;

class SoundsManagerTestImpl : public audio::SoundsManager {
 public:
  SoundsManagerTestImpl() = default;

  SoundsManagerTestImpl(const SoundsManagerTestImpl&) = delete;
  SoundsManagerTestImpl& operator=(const SoundsManagerTestImpl&) = delete;

  ~SoundsManagerTestImpl() override {}

  bool Initialize(SoundKey key,
                  std::string_view /* data */,
                  media::AudioCodec codec) override {
    is_sound_initialized_[key] = true;
    return true;
  }

  bool Play(SoundKey key) override {
    ++num_play_requests_[key];
    return true;
  }

  bool Stop(SoundKey key) override { return true; }

  base::TimeDelta GetDuration(SoundKey /* key */) override {
    return base::TimeDelta();
  }

  bool is_sound_initialized(SoundKey key) { return is_sound_initialized_[key]; }

  int num_play_requests(SoundKey key) { return num_play_requests_[key]; }

 private:
  std::map<SoundKey, bool> is_sound_initialized_;
  std::map<SoundKey, int> num_play_requests_;
};

class VolumeControllerTest : public InProcessBrowserTest {
 public:
  VolumeControllerTest() = default;

  VolumeControllerTest(const VolumeControllerTest&) = delete;
  VolumeControllerTest& operator=(const VolumeControllerTest&) = delete;

  ~VolumeControllerTest() override {}

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
  VolumeControllerSoundsTest() : sounds_manager_(nullptr) {}

  VolumeControllerSoundsTest(const VolumeControllerSoundsTest&) = delete;
  VolumeControllerSoundsTest& operator=(const VolumeControllerSoundsTest&) =
      delete;

  ~VolumeControllerSoundsTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    sounds_manager_ = new SoundsManagerTestImpl();
    audio::SoundsManager::InitializeForTesting(sounds_manager_);
  }

  bool is_sound_initialized() const {
    return sounds_manager_->is_sound_initialized(
        static_cast<int>(ash::Sound::kVolumeAdjust));
  }

  int num_play_requests() const {
    return sounds_manager_->num_play_requests(
        static_cast<int>(ash::Sound::kVolumeAdjust));
  }

 private:
  raw_ptr<SoundsManagerTestImpl, DanglingUntriaged> sounds_manager_;
};

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsTest, Simple) {
  audio_handler_->SetOutputVolumePercent(50);

  AccessibilityManager::Get()->EnableSpokenFeedback(false);
  VolumeUp();
  VolumeDown();
  EXPECT_EQ(0, num_play_requests());

  AccessibilityManager::Get()->EnableSpokenFeedback(true);
  VolumeUp();
  VolumeDown();
  EXPECT_EQ(2, num_play_requests());
}

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsTest, EdgeCases) {
  EXPECT_TRUE(is_sound_initialized());
  AccessibilityManager::Get()->EnableSpokenFeedback(true);

  // Check that sound is played on volume up and volume down.
  audio_handler_->SetOutputVolumePercent(50);
  VolumeUp();
  EXPECT_EQ(1, num_play_requests());
  VolumeDown();
  EXPECT_EQ(2, num_play_requests());

  audio_handler_->SetOutputVolumePercent(99);
  VolumeUp();
  EXPECT_EQ(3, num_play_requests());

  audio_handler_->SetOutputVolumePercent(100);
  VolumeUp();
  EXPECT_EQ(3, num_play_requests());

  // Check that sound isn't played when audio is muted.
  audio_handler_->SetOutputVolumePercent(50);
  VolumeMute();
  VolumeDown();
  ASSERT_TRUE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(3, num_play_requests());

  // Check that audio is unmuted and sound is played.
  VolumeUp();
  ASSERT_FALSE(audio_handler_->IsOutputMuted());
  EXPECT_EQ(4, num_play_requests());
}

class VolumeControllerSoundsDisabledTest : public VolumeControllerSoundsTest {
 public:
  VolumeControllerSoundsDisabledTest() = default;

  VolumeControllerSoundsDisabledTest(
      const VolumeControllerSoundsDisabledTest&) = delete;
  VolumeControllerSoundsDisabledTest& operator=(
      const VolumeControllerSoundsDisabledTest&) = delete;

  ~VolumeControllerSoundsDisabledTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    VolumeControllerSoundsTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(ash::switches::kDisableVolumeAdjustSound);
  }
};

IN_PROC_BROWSER_TEST_F(VolumeControllerSoundsDisabledTest, VolumeAdjustSounds) {
  EXPECT_FALSE(is_sound_initialized());

  // Check that sound isn't played on volume up and volume down.
  audio_handler_->SetOutputVolumePercent(50);
  VolumeUp();
  VolumeDown();
  EXPECT_EQ(0, num_play_requests());
}

}  // namespace
