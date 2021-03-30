// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_audio_detailed_view_controller.h"

#include "ash/components/audio/audio_devices_pref_handler.h"
#include "ash/components/audio/audio_devices_pref_handler_stub.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/system/audio/audio_detailed_view.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/audio/fake_cras_audio_client.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/media_session/public/mojom/media_controller.mojom-test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using chromeos::AudioNode;
using chromeos::AudioNodeList;

namespace ash {
namespace {

class FakeMediaControllerManager
    : public media_session::mojom::MediaControllerManagerInterceptorForTesting {
 public:
  FakeMediaControllerManager() = default;

  mojo::PendingRemote<media_session::mojom::MediaControllerManager>
  MakeRemote() {
    mojo::PendingRemote<media_session::mojom::MediaControllerManager> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  // media_session::mojom::MediaControllerManagerInterceptorForTesting:
  media_session::mojom::MediaControllerManager* GetForwardingInterface()
      override {
    NOTREACHED();
    return nullptr;
  }

  void CreateActiveMediaController(
      mojo::PendingReceiver<media_session::mojom::MediaController> receiver)
      override {}

  MOCK_METHOD0(SuspendAllSessions, void());

 private:
  mojo::ReceiverSet<media_session::mojom::MediaControllerManager> receivers_;
};

constexpr uint64_t kMicJackId = 10010;
constexpr uint64_t kInternalMicId = 10003;
constexpr uint64_t kFrontMicId = 10012;
constexpr uint64_t kRearMicId = 10013;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const AudioNodeInfo kMicJack[] = {
    {true, kMicJackId, "Fake Mic Jack", "MIC", "Mic Jack"}};

const AudioNodeInfo kInternalMic[] = {
    {true, kInternalMicId, "Fake Mic", "INTERNAL_MIC", "Internal Mic"}};

const AudioNodeInfo kFrontMic[] = {
    {true, kFrontMicId, "Fake Front Mic", "FRONT_MIC", "Front Mic"}};

const AudioNodeInfo kRearMic[] = {
    {true, kRearMicId, "Fake Rear Mic", "REAR_MIC", "Rear Mic"}};

AudioNode GenerateAudioNode(const AudioNodeInfo* node_info) {
  uint64_t stable_device_id_v2 = 0;
  uint64_t stable_device_id_v1 = node_info->id;
  return AudioNode(node_info->is_input, node_info->id, false,
                   stable_device_id_v1, stable_device_id_v2,
                   node_info->device_name, node_info->type, node_info->name,
                   false /* is_active*/, 0 /* pluged_time */,
                   node_info->is_input ? kInputMaxSupportedChannels
                                       : kOutputMaxSupportedChannels);
}

AudioNodeList GenerateAudioNodeList(
    const std::vector<const AudioNodeInfo*>& nodes) {
  AudioNodeList node_list;
  for (auto* node_info : nodes) {
    node_list.push_back(GenerateAudioNode(node_info));
  }
  return node_list;
}

}  // namespace

// Test param is the version of stabel device id used by audio node.
class UnifiedAudioDetailedViewControllerTest : public AshTestBase {
 public:
  UnifiedAudioDetailedViewControllerTest() {}
  ~UnifiedAudioDetailedViewControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    fake_manager_ = std::make_unique<FakeMediaControllerManager>();
    tray_model_ = std::make_unique<UnifiedSystemTrayModel>(nullptr);
    tray_controller_ =
        std::make_unique<UnifiedSystemTrayController>(tray_model_.get());
    audio_detailed_view_controller_ =
        std::make_unique<UnifiedAudioDetailedViewController>(
            tray_controller_.get());

    map_device_sliders_callback_ = base::BindRepeating(
        &UnifiedAudioDetailedViewControllerTest::AddViewToSliderDeviceMap,
        base::Unretained(this));
    MicGainSliderController::SetMapDeviceSliderCallbackForTest(
        &map_device_sliders_callback_);
  }

  void TearDown() override {
    MicGainSliderController::SetMapDeviceSliderCallbackForTest(nullptr);
    audio_pref_handler_ = nullptr;

    audio_detailed_view_controller_.reset();
    tray_controller_.reset();
    tray_model_.reset();

    fake_manager_.reset();
    AshTestBase::TearDown();
  }

  void SetUpCrasAudioHandler(const AudioNodeList& audio_nodes) {
    // Shutdown previous instance in case there is one.
    if (CrasAudioHandler::Get())
      CrasAudioHandler::Shutdown();

    fake_cras_audio_client()->SetAudioNodesForTesting(audio_nodes);
    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    CrasAudioHandler::Initialize(fake_manager_->MakeRemote(),
                                 audio_pref_handler_);
    cras_audio_handler_ = CrasAudioHandler::Get();
  }

  void AddViewToSliderDeviceMap(uint64_t device_id, views::View* view) {
    sliders_map_[device_id] = view;
  }

 protected:
  chromeos::FakeCrasAudioClient* fake_cras_audio_client() {
    return chromeos::FakeCrasAudioClient::Get();
  }

  std::map<uint64_t, views::View*> sliders_map_;
  MicGainSliderController::MapDeviceSliderCallback map_device_sliders_callback_;
  CrasAudioHandler* cras_audio_handler_ = nullptr;  // Not owned.
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
  std::unique_ptr<FakeMediaControllerManager> fake_manager_;
  std::unique_ptr<UnifiedAudioDetailedViewController>
      audio_detailed_view_controller_;
  std::unique_ptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(UnifiedAudioDetailedViewControllerTest, OnlyOneVisibleSlider) {
  SetUpCrasAudioHandler(GenerateAudioNodeList({kInternalMic, kMicJack}));
  audio_detailed_view_controller_->CreateView();

  // Only slider corresponding to the Internal Mic should be visible initially.
  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      CrasAudioHandler::ACTIVATE_BY_USER);
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(sliders_map_.find(kInternalMicId)->second->GetVisible());

  EXPECT_FALSE(sliders_map_.find(kMicJackId)->second->GetVisible());

  // Switching to Mic Jack should flip the visibility of the sliders.
  cras_audio_handler_->SwitchToDevice(AudioDevice(GenerateAudioNode(kMicJack)),
                                      true, CrasAudioHandler::ACTIVATE_BY_USER);
  EXPECT_EQ(kMicJackId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(sliders_map_.find(kMicJackId)->second->GetVisible());

  EXPECT_FALSE(sliders_map_.find(kInternalMicId)->second->GetVisible());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       DualInternalMicHasSingleVisibleSlider) {
  SetUpCrasAudioHandler(GenerateAudioNodeList({kFrontMic, kRearMic}));

  // Verify the device has dual internal mics.
  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  audio_detailed_view_controller_->CreateView();

  // Verify there is only 1 slider in the view.
  EXPECT_EQ(sliders_map_.size(), 1u);

  // Verify the slider is visible.
  EXPECT_TRUE(sliders_map_.begin()->second->GetVisible());
}

}  // namespace ash
