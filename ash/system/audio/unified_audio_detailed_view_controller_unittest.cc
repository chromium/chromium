// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_audio_detailed_view_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/components/audio/audio_devices_pref_handler.h"
#include "ash/components/audio/audio_devices_pref_handler_stub.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/audio/audio_detailed_view.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/audio/fake_cras_audio_client.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

using chromeos::AudioNode;
using chromeos::AudioNodeList;

namespace ash {
namespace {

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
  const uint32_t audio_effect;
};

const uint32_t kInputMaxSupportedChannels = 1;
const uint32_t kOutputMaxSupportedChannels = 2;

const AudioNodeInfo kMicJack[] = {
    {true, kMicJackId, "Fake Mic Jack", "MIC", "Mic Jack", 0}};

const AudioNodeInfo kInternalMic[] = {{true, kInternalMicId, "Fake Mic",
                                       "INTERNAL_MIC", "Internal Mic",
                                       cras::EFFECT_TYPE_NOISE_CANCELLATION}};

const AudioNodeInfo kFrontMic[] = {
    {true, kFrontMicId, "Fake Front Mic", "FRONT_MIC", "Front Mic", 0}};

const AudioNodeInfo kRearMic[] = {
    {true, kRearMicId, "Fake Rear Mic", "REAR_MIC", "Rear Mic", 0}};

AudioNode GenerateAudioNode(const AudioNodeInfo* node_info) {
  uint64_t stable_device_id_v2 = 0;
  uint64_t stable_device_id_v1 = node_info->id;
  return AudioNode(node_info->is_input, node_info->id, false,
                   stable_device_id_v1, stable_device_id_v2,
                   node_info->device_name, node_info->type, node_info->name,
                   false /* is_active*/, 0 /* pluged_time */,
                   node_info->is_input ? kInputMaxSupportedChannels
                                       : kOutputMaxSupportedChannels,
                   node_info->audio_effect);
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

    audio_pref_handler_ = base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
    cras_audio_handler_ = CrasAudioHandler::Get();
    cras_audio_handler_->SetPrefHandlerForTesting(audio_pref_handler_);

    tray_model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
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

    noise_cancellation_toggle_callback_ =
        base::BindRepeating(&UnifiedAudioDetailedViewControllerTest::
                                AddViewToNoiseCancellationToggleMap,
                            base::Unretained(this));
    tray::AudioDetailedView::SetMapNoiseCancellationToggleCallbackForTest(
        &noise_cancellation_toggle_callback_);
  }

  void TearDown() override {
    MicGainSliderController::SetMapDeviceSliderCallbackForTest(nullptr);
    audio_pref_handler_ = nullptr;
    audio_detailed_view_ = nullptr;
    audio_detailed_view_.reset();
    audio_detailed_view_controller_.reset();
    tray_controller_.reset();
    tray_model_.reset();

    AshTestBase::TearDown();
  }

  void AddViewToSliderDeviceMap(uint64_t device_id, views::View* view) {
    sliders_map_[device_id] = view;
  }

  void AddViewToNoiseCancellationToggleMap(uint64_t device_id,
                                           views::View* view) {
    toggles_map_[device_id] = view;
  }

  void ToggleLiveCaption() {
    audio_detailed_view()->HandleViewClicked(live_caption_view());
  }

 protected:
  chromeos::FakeCrasAudioClient* fake_cras_audio_client() {
    return chromeos::FakeCrasAudioClient::Get();
  }

  tray::AudioDetailedView* audio_detailed_view() {
    if (!audio_detailed_view_) {
      audio_detailed_view_ =
          base::WrapUnique(static_cast<tray::AudioDetailedView*>(
              audio_detailed_view_controller_->CreateView()));
    }
    return audio_detailed_view_.get();
  }

  views::View* live_caption_view() {
    return audio_detailed_view()->live_caption_view_;
  }

  bool live_caption_enabled() {
    return Shell::Get()->accessibility_controller()->live_caption().enabled();
  }

  std::map<uint64_t, views::View*> sliders_map_;
  std::map<uint64_t, views::View*> toggles_map_;
  MicGainSliderController::MapDeviceSliderCallback map_device_sliders_callback_;
  tray::AudioDetailedView::NoiseCancellationCallback
      noise_cancellation_toggle_callback_;
  CrasAudioHandler* cras_audio_handler_ = nullptr;  // Not owned.
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
  std::unique_ptr<UnifiedAudioDetailedViewController>
      audio_detailed_view_controller_;
  scoped_refptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<tray::AudioDetailedView> audio_detailed_view_;
};

TEST_F(UnifiedAudioDetailedViewControllerTest, OnlyOneVisibleSlider) {
  std::unique_ptr<views::View> view =
      base::WrapUnique(audio_detailed_view_controller_->CreateView());
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack}));

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
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kFrontMic, kRearMic}));

  // Verify the device has dual internal mics.
  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  std::unique_ptr<views::View> view =
      base::WrapUnique(audio_detailed_view_controller_->CreateView());

  // Verify there is only 1 slider in the view.
  EXPECT_EQ(sliders_map_.size(), 1u);

  // Verify the slider is visible.
  EXPECT_TRUE(sliders_map_.begin()->second->GetVisible());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationToggleNotDisplayedIfNotSupported) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableInputNoiseCancellationUi);
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(false);

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      CrasAudioHandler::ACTIVATE_BY_USER);

  std::unique_ptr<views::View> view =
      base::WrapUnique(audio_detailed_view_controller_->CreateView());
  EXPECT_EQ(0u, toggles_map_.size());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationToggleDisplayedIfSupportedAndInternal) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableInputNoiseCancellationUi);

  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);

  auto internal_mic = AudioDevice(GenerateAudioNode(kInternalMic));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);

  std::unique_ptr<views::View> view =
      base::WrapUnique(audio_detailed_view_controller_->CreateView());
  EXPECT_EQ(1u, toggles_map_.size());

  views::ToggleButton* toggle =
      (views::ToggleButton*)toggles_map_[internal_mic.id]->children()[1];
  EXPECT_TRUE(toggle->GetIsOn());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationToggleChangesPrefAndSendsDbusSignal) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableInputNoiseCancellationUi);

  audio_pref_handler_->SetNoiseCancellationState(false);

  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);

  auto internal_mic = AudioDevice(GenerateAudioNode(kInternalMic));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      CrasAudioHandler::ACTIVATE_BY_USER);

  std::unique_ptr<views::View> view =
      base::WrapUnique(audio_detailed_view_controller_->CreateView());
  EXPECT_EQ(1u, toggles_map_.size());

  views::ToggleButton* toggle =
      (views::ToggleButton*)toggles_map_[internal_mic.id]->children()[1];
  auto widget = CreateFramelessTestWidget();
  widget->SetContentsView(toggle);

  // The toggle loaded the pref correctly.
  EXPECT_FALSE(toggle->GetIsOn());
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());

  ui::MouseEvent press(ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_NONE);

  // Flipping the toggle.
  views::test::ButtonTestApi(toggle).NotifyClick(press);
  // The new state of the toggle must be saved to the prefs.
  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());

  // Flipping back and checking the prefs again.
  views::test::ButtonTestApi(toggle).NotifyClick(press);
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
}

// TODO(1205197): Remove this test once the flag is removed.
TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationToggleNotDisplayedIfFlagIsOff) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kEnableInputNoiseCancellationUi);
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      CrasAudioHandler::ACTIVATE_BY_USER);

  std::unique_ptr<views::View> view =
      base::WrapUnique(audio_detailed_view_controller_->CreateView());
  EXPECT_EQ(0u, toggles_map_.size());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationUpdatedWhenDeviceChanges) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kEnableInputNoiseCancellationUi);

  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      CrasAudioHandler::ACTIVATE_BY_USER);

  EXPECT_EQ(0u, fake_cras_audio_client()->GetNoiseCancellationEnabledCount());

  std::unique_ptr<views::View> view =
      base::WrapUnique(audio_detailed_view_controller_->CreateView());

  EXPECT_EQ(1u, toggles_map_.size());
  // audio_detailed_view_controller_->CreateView() calls
  // AudioDetailedView::Update() twice, so SetNoiseCancellationEnabled is called
  // twice.
  EXPECT_EQ(2u, fake_cras_audio_client()->GetNoiseCancellationEnabledCount());

  cras_audio_handler_->SwitchToDevice(AudioDevice(GenerateAudioNode(kMicJack)),
                                      true, CrasAudioHandler::ACTIVATE_BY_USER);
  EXPECT_EQ(3u, fake_cras_audio_client()->GetNoiseCancellationEnabledCount());
}

TEST_F(UnifiedAudioDetailedViewControllerTest, ToggleLiveCaption) {
  scoped_feature_list_.InitWithFeatures(
      {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
       ash::features::kOnDeviceSpeechRecognition},
      {});

  EXPECT_TRUE(live_caption_view());
  EXPECT_FALSE(live_caption_enabled());

  ToggleLiveCaption();
  EXPECT_TRUE(live_caption_view());
  EXPECT_TRUE(live_caption_enabled());

  ToggleLiveCaption();
  EXPECT_TRUE(live_caption_view());
  EXPECT_FALSE(live_caption_enabled());
}

TEST_F(UnifiedAudioDetailedViewControllerTest, LiveCaptionNotAvailable) {
  // If the Live Caption feature flags are not set, the Live Caption toggle will
  // not appear in audio settings.
  EXPECT_FALSE(live_caption_view());
  EXPECT_FALSE(live_caption_enabled());
}

}  // namespace ash
