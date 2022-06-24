// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/audio/cros_audio_config_impl.h"

#include "ash/components/audio/audio_devices_pref_handler.h"
#include "ash/components/audio/audio_devices_pref_handler_stub.h"
#include "ash/components/audio/cras_audio_handler.h"
#include "ash/components/audio/cros_audio_config.h"
#include "ash/components/audio/public/mojom/cros_audio_config.mojom.h"
#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::audio_config {

const uint8_t kTestOutputVolumePercent = 80u;
const int8_t kDefaultOutputVolumePercent =
    AudioDevicesPrefHandler::kDefaultOutputVolumePercent;

class FakeAudioSystemPropertiesObserver
    : public mojom::AudioSystemPropertiesObserver {
 public:
  FakeAudioSystemPropertiesObserver() = default;
  ~FakeAudioSystemPropertiesObserver() override = default;

  mojo::PendingRemote<AudioSystemPropertiesObserver> GeneratePendingRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnPropertiesUpdated(
      mojom::AudioSystemPropertiesPtr properties) override {
    last_audio_system_properties_ = std::move(properties);
    ++num_properties_updated_calls_;
  };

  absl::optional<mojom::AudioSystemPropertiesPtr> last_audio_system_properties_;
  size_t num_properties_updated_calls_ = 0u;
  mojo::Receiver<mojom::AudioSystemPropertiesObserver> receiver_{this};
};

class CrosAudioConfigImplTest : public testing::Test {
 public:
  CrosAudioConfigImplTest() = default;
  ~CrosAudioConfigImplTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kAudioSettingsPage);
    CrasAudioClient::InitializeFake();
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    CrasAudioHandler::Initialize(mojo::NullRemote(), audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
    cros_audio_config_ = std::make_unique<CrosAudioConfigImpl>();
  }

  void TearDown() override {
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
    audio_pref_handler_ = nullptr;
  }

 protected:
  std::unique_ptr<FakeAudioSystemPropertiesObserver> Observe() {
    cros_audio_config_->BindPendingReceiver(
        remote_.BindNewPipeAndPassReceiver());
    auto fake_observer = std::make_unique<FakeAudioSystemPropertiesObserver>();
    remote_->ObserveAudioSystemProperties(
        fake_observer->GeneratePendingRemote());
    base::RunLoop().RunUntilIdle();
    return fake_observer;
  }

  void SetOutputVolumePercent(uint8_t volume_percent) {
    cras_audio_handler_->SetOutputVolumePercent(volume_percent);
    base::RunLoop().RunUntilIdle();
  }

  void SetOutputMuteState(mojom::MuteState mute_state) {
    switch (mute_state) {
      case mojom::MuteState::kMutedByUser:
        audio_pref_handler_->SetAudioOutputAllowedValue(true);
        cras_audio_handler_->SetOutputMute(true);
        break;
      case mojom::MuteState::kNotMuted:
        audio_pref_handler_->SetAudioOutputAllowedValue(true);
        cras_audio_handler_->SetOutputMute(false);
        break;
      case mojom::MuteState::kMutedByPolicy:
        // Calling this method does not alert AudioSystemPropertiesObserver.
        audio_pref_handler_->SetAudioOutputAllowedValue(false);
        break;
    }
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;

  CrasAudioHandler* cras_audio_handler_ = nullptr;  // Not owned.
  std::unique_ptr<CrosAudioConfigImpl> cros_audio_config_;
  mojo::Remote<mojom::CrosAudioConfig> remote_;
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
};

TEST_F(CrosAudioConfigImplTest, GetOutputVolumePercent) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  // |fake_observer| count is first incremented in Observe() method.
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  ASSERT_EQ(kDefaultOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);

  SetOutputVolumePercent(kTestOutputVolumePercent);
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(kTestOutputVolumePercent,
            fake_observer->last_audio_system_properties_.value()
                ->output_volume_percent);
}

TEST_F(CrosAudioConfigImplTest, GetOutputMuteState) {
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputMuteState(mojom::MuteState::kMutedByUser);
  ASSERT_EQ(2u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kMutedByUser,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);

  SetOutputMuteState(mojom::MuteState::kNotMuted);
  ASSERT_EQ(3u, fake_observer->num_properties_updated_calls_);
  EXPECT_EQ(
      mojom::MuteState::kNotMuted,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
}

TEST_F(CrosAudioConfigImplTest, GetOutputMuteStateMutedByPolicy) {
  SetOutputMuteState(mojom::MuteState::kMutedByPolicy);
  std::unique_ptr<FakeAudioSystemPropertiesObserver> fake_observer = Observe();
  ASSERT_EQ(1u, fake_observer->num_properties_updated_calls_);
  ASSERT_TRUE(fake_observer->last_audio_system_properties_.has_value());
  EXPECT_EQ(
      mojom::MuteState::kMutedByPolicy,
      fake_observer->last_audio_system_properties_.value()->output_mute_state);
}

}  // namespace ash::audio_config
