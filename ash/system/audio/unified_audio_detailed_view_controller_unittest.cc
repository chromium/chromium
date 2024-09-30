// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/unified_audio_detailed_view_controller.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/switch.h"
#include "ash/system/audio/audio_detailed_view.h"
#include "ash/system/audio/mic_gain_slider_controller.h"
#include "ash/system/audio/mic_gain_slider_view.h"
#include "ash/system/audio/unified_volume_slider_controller.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/unified/unified_slider_view.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "components/live_caption/pref_names.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr uint64_t kMicJackId = 10010;
constexpr uint64_t kInternalMicId = 10003;
constexpr uint64_t kInternalMicStyleTransferId = 10004;
constexpr uint64_t kFrontMicId = 10012;
constexpr uint64_t kRearMicId = 10013;
constexpr uint64_t kNbsMicId = 10020;
const uint64_t kInternalSpeakerId = 10001;
const uint64_t kHeadphoneId = 10002;
constexpr uint64_t kDualInternalMicId = 0;
const int kFrontMicGainPercent = 50;
const int kRearMicGainPercent = 100;

const std::u16string kInitialLiveCaptionViewSubtitleText = u"This is a test";
const std::u16string kSodaDownloaded = u"Speech files downloaded";
const std::u16string kSodaInProgress25 =
    u"Downloading speech recognition files… 25%";
const std::u16string kSodaInProgress50 =
    u"Downloading speech recognition files… 50%";
const std::u16string kSodaFailed =
    u"Can't download speech files. Try again later.";

speech::LanguageCode en_us() {
  return speech::LanguageCode::kEnUs;
}

speech::LanguageCode fr_fr() {
  return speech::LanguageCode::kFrFr;
}

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

const int32_t kInputNumberOfVolumeSteps = 0;
const int32_t kOutputNumberOfVolumeSteps = 25;

const AudioNodeInfo kNbsMic[] = {
    {true, kNbsMicId, "Fake Nbs Mic", "BLUETOOTH_NB_MIC", "Nbs Mic", 0}};

const AudioNodeInfo kMicJack[] = {
    {true, kMicJackId, "Fake Mic Jack", "MIC", "Mic Jack", 0}};

const AudioNodeInfo kInternalMic[] = {{true, kInternalMicId, "Fake Mic",
                                       "INTERNAL_MIC", "Internal Mic",
                                       cras::EFFECT_TYPE_NOISE_CANCELLATION}};

const AudioNodeInfo kInternalMicStyleTransfer[] = {
    {true, kInternalMicStyleTransferId, "Fake Mic", "INTERNAL_MIC", "Internal Mic",
     cras::EFFECT_TYPE_STYLE_TRANSFER}};

const AudioNodeInfo kFrontMic[] = {
    {true, kFrontMicId, "Fake Front Mic", "FRONT_MIC", "Front Mic", 0}};

const AudioNodeInfo kRearMic[] = {
    {true, kRearMicId, "Fake Rear Mic", "REAR_MIC", "Rear Mic", 0}};

const AudioNodeInfo kInternalSpeaker[] = {{false, kInternalSpeakerId,
                                           "Fake Speaker", "INTERNAL_SPEAKER",
                                           "Speaker", 0}};

const AudioNodeInfo kHeadphone[] = {
    {false, kHeadphoneId, "Fake Headphone", "HEADPHONE", "Headphone", 0}};

AudioNode GenerateAudioNode(const AudioNodeInfo* node_info) {
  uint64_t stable_device_id_v2 = 0;
  uint64_t stable_device_id_v1 = node_info->id;
  return AudioNode(node_info->is_input, node_info->id, false,
                   stable_device_id_v1, stable_device_id_v2,
                   node_info->device_name, node_info->type, node_info->name,
                   false /* is_active*/, 0 /* pluged_time */,
                   node_info->is_input ? kInputMaxSupportedChannels
                                       : kOutputMaxSupportedChannels,
                   node_info->audio_effect,
                   node_info->is_input ? kInputNumberOfVolumeSteps
                                       : kOutputNumberOfVolumeSteps);
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

// Test param is the version of stable device id used by audio node.
class UnifiedAudioDetailedViewControllerTest : public AshTestBase {
 public:
  UnifiedAudioDetailedViewControllerTest() = default;
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

    map_input_device_sliders_callback_ = base::BindRepeating(
        &UnifiedAudioDetailedViewControllerTest::AddViewToInputSliderDeviceMap,
        base::Unretained(this));
    MicGainSliderController::SetMapDeviceSliderCallbackForTest(
        &map_input_device_sliders_callback_);

    map_output_device_sliders_callback_ = base::BindRepeating(
        &UnifiedAudioDetailedViewControllerTest::AddViewToOutputSliderDeviceMap,
        base::Unretained(this));
    UnifiedVolumeSliderController::SetMapDeviceSliderCallbackForTest(
        &map_output_device_sliders_callback_);

    noise_cancellation_toggle_callback_ =
        base::BindRepeating(&UnifiedAudioDetailedViewControllerTest::
                                AddViewToNoiseCancellationToggleMap,
                            base::Unretained(this));
    AudioDetailedView::SetMapNoiseCancellationToggleCallbackForTest(
        &noise_cancellation_toggle_callback_);

    style_transfer_toggle_callback_ =
        base::BindRepeating(&UnifiedAudioDetailedViewControllerTest::
                                AddViewToStyleTransferToggleMap,
                            base::Unretained(this));
    AudioDetailedView::SetMapStyleTransferToggleCallbackForTest(
        &style_transfer_toggle_callback_);
  }

  void TearDown() override {
    MicGainSliderController::SetMapDeviceSliderCallbackForTest(nullptr);
    UnifiedVolumeSliderController::SetMapDeviceSliderCallbackForTest(nullptr);
    audio_pref_handler_ = nullptr;
    audio_detailed_view_ = nullptr;
    audio_detailed_view_.reset();
    audio_detailed_view_controller_.reset();
    tray_controller_.reset();
    tray_model_.reset();

    AshTestBase::TearDown();
  }

  void AddViewToInputSliderDeviceMap(uint64_t device_id, views::View* view) {
    input_sliders_map_[device_id] = view;
  }

  void AddViewToOutputSliderDeviceMap(uint64_t device_id, views::View* view) {
    output_sliders_map_[device_id] = view;
  }

  void AddViewToNoiseCancellationToggleMap(uint64_t device_id,
                                           views::View* view) {
    toggles_map_[device_id] = view;
  }

  void AddViewToStyleTransferToggleMap(uint64_t device_id, views::View* view) {
    style_transfer_toggles_map_[device_id] = view;
  }

  void ToggleLiveCaption() {
    GetAudioDetailedView()->HandleViewClicked(live_caption_view());
  }

  // Toggles the noise cancellation button.
  void ToggleNoiseCancellation() {
    GetAudioDetailedView()->HandleViewClicked(
        GetAudioDetailedView()->noise_cancellation_view_);
  }

  // Toggles the style transfer button.
  void ToggleStyleTransfer(AudioDetailedView* audio_detailed_view = nullptr) {
    AudioDetailedView* view =
        audio_detailed_view ? audio_detailed_view : GetAudioDetailedView();
    view->HandleViewClicked(view->style_transfer_view_);
  }

 protected:
  FakeCrasAudioClient* fake_cras_audio_client() {
    return FakeCrasAudioClient::Get();
  }

  AudioDetailedView* GetAudioDetailedView() {
    if (!audio_detailed_view_) {
      audio_detailed_view_ = base::WrapUnique(static_cast<AudioDetailedView*>(
          audio_detailed_view_controller_->CreateView().release()));
    }
    return audio_detailed_view_.get();
  }

  void CheckSliderFocusBehavior(bool is_input_slider, uint64_t device_id) {
    SCOPED_TRACE(
        base::StringPrintf("Test params: is_input_slider=%d", is_input_slider));

    auto sliders_map =
        is_input_slider ? input_sliders_map_ : output_sliders_map_;
    EXPECT_TRUE(base::Contains(sliders_map, device_id));

    auto* unified_slider_view =
        static_cast<UnifiedSliderView*>(sliders_map.find(device_id)->second);

    views::Slider* slider = unified_slider_view->slider();
    IconButton* slider_button = unified_slider_view->slider_button();
    // `slider` is normally focusable, and `slider_button` is accessibility
    // focusable.
    EXPECT_TRUE(slider->IsFocusable());
    EXPECT_FALSE(slider_button->IsFocusable());
    EXPECT_TRUE(
        slider_button->GetViewAccessibility().IsAccessibilityFocusable());

    slider->RequestFocus();
    EXPECT_STREQ(slider->GetFocusManager()->GetFocusedView()->GetClassName(),
                 "QuickSettingsSlider");
    // Check the accessibility role of QuickSettingsSlider.
    ui::AXNodeData node_data;
    slider->GetFocusManager()
        ->GetFocusedView()
        ->GetViewAccessibility()
        .GetAccessibleNodeData(&node_data);
    EXPECT_EQ(node_data.role, ax::mojom::Role::kSlider);

    const bool is_muted =
        is_input_slider
            ? cras_audio_handler_->IsInputMutedForDevice(device_id)
            : cras_audio_handler_->IsOutputMutedForDevice(device_id);

    // Presses the enter key when focused on the slider will toggle mute state.
    GetEventGenerator()->PressAndReleaseKey(ui::KeyboardCode::VKEY_RETURN);
    if (is_input_slider) {
      EXPECT_EQ(cras_audio_handler_->IsInputMutedForDevice(device_id),
                !is_muted);
    } else {
      EXPECT_EQ(cras_audio_handler_->IsOutputMutedForDevice(device_id),
                !is_muted);
    }
  }

  HoverHighlightView* live_caption_view() {
    return GetAudioDetailedView()->live_caption_view_;
  }

  views::ToggleButton* noise_cancellation_button() {
    return GetAudioDetailedView()->noise_cancellation_button_;
  }

  views::ToggleButton* style_transfer_button() {
    return GetAudioDetailedView()->style_transfer_button_;
  }

  bool live_caption_enabled() {
    return Shell::Get()->accessibility_controller()->live_caption().enabled();
  }

  views::View* nbs_warning_view() {
    return GetAudioDetailedView()->GetViewByID(
        AudioDetailedView::AudioDetailedViewID::kNbsWarningView);
  }

  std::map<uint64_t, raw_ptr<views::View, CtnExperimental>> input_sliders_map_;
  std::map<uint64_t, raw_ptr<views::View, CtnExperimental>> output_sliders_map_;
  std::map<uint64_t, raw_ptr<views::View, CtnExperimental>> toggles_map_;
  std::map<uint64_t, raw_ptr<views::View, CtnExperimental>>
      style_transfer_toggles_map_;
  MicGainSliderController::MapDeviceSliderCallback
      map_input_device_sliders_callback_;
  UnifiedVolumeSliderController::MapDeviceSliderCallback
      map_output_device_sliders_callback_;
  AudioDetailedView::NoiseCancellationCallback
      noise_cancellation_toggle_callback_;
  AudioDetailedView::StyleTransferCallback style_transfer_toggle_callback_;
  raw_ptr<CrasAudioHandler, DanglingUntriaged> cras_audio_handler_ =
      nullptr;  // Not owned.
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_;
  std::unique_ptr<UnifiedAudioDetailedViewController>
      audio_detailed_view_controller_;
  scoped_refptr<UnifiedSystemTrayModel> tray_model_;
  std::unique_ptr<UnifiedSystemTrayController> tray_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<AudioDetailedView> audio_detailed_view_;
  base::HistogramTester histogram_tester_;
};

TEST_F(UnifiedAudioDetailedViewControllerTest, ToggleNbsWarning) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kMicJack, kNbsMic}));

  auto jack_mic = AudioDevice(GenerateAudioNode(kMicJack));
  cras_audio_handler_->SwitchToDevice(jack_mic, true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_FALSE(nbs_warning_view());

  auto nbs_mic = AudioDevice(GenerateAudioNode(kNbsMic));
  cras_audio_handler_->SwitchToDevice(nbs_mic, true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_TRUE(nbs_warning_view());

  cras_audio_handler_->SwitchToDevice(jack_mic, true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_FALSE(nbs_warning_view());
}

TEST_F(UnifiedAudioDetailedViewControllerTest, OneInputSlider) {
  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack}));

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kInternalMicId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(input_sliders_map_.find(kInternalMicId)->second->GetVisible());

  // Both sliders should be visible.
  EXPECT_TRUE(input_sliders_map_.find(kMicJackId)->second->GetVisible());

  cras_audio_handler_->SwitchToDevice(AudioDevice(GenerateAudioNode(kMicJack)),
                                      true,
                                      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kMicJackId, cras_audio_handler_->GetPrimaryActiveInputNode());
  EXPECT_TRUE(input_sliders_map_.find(kMicJackId)->second->GetVisible());

  // Both sliders should be visible.
  EXPECT_TRUE(input_sliders_map_.find(kInternalMicId)->second->GetVisible());
}

TEST_F(UnifiedAudioDetailedViewControllerTest, OneOutputSlider) {
  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone}));

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), true,
      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kInternalSpeakerId,
            cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Both sliders should be visible.
  EXPECT_TRUE(
      output_sliders_map_.find(kInternalSpeakerId)->second->GetVisible());
  EXPECT_TRUE(output_sliders_map_.find(kHeadphoneId)->second->GetVisible());

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kHeadphone)), true,
      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kHeadphoneId, cras_audio_handler_->GetPrimaryActiveOutputNode());
  // Both sliders should be visible.
  EXPECT_TRUE(output_sliders_map_.find(kHeadphoneId)->second->GetVisible());
  EXPECT_TRUE(
      output_sliders_map_.find(kInternalSpeakerId)->second->GetVisible());
}

TEST_F(UnifiedAudioDetailedViewControllerTest, DualInternalMicSlider) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kFrontMic, kRearMic}));

  // Verify the device has dual internal mics.
  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();

  // Verify there is only 1 slider in the view.
  EXPECT_EQ(input_sliders_map_.size(), 1u);

  // Verify the slider is visible.
  EXPECT_TRUE(input_sliders_map_.begin()->second->GetVisible());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       DualInternalMicSliderActiveState) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kFrontMic, kRearMic}));

  // Verify the device has dual internal mics.
  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();

  // Verify there is only 1 slider in the view.
  EXPECT_EQ(input_sliders_map_.size(), 1u);

  auto* mic_gain_slider_view =
      static_cast<MicGainSliderView*>(input_sliders_map_.begin()->second);

  // Verify the slider is visible.
  EXPECT_TRUE(mic_gain_slider_view->GetVisible());

  // Verifies the slider is active since it's the only slider.
  EXPECT_EQ(static_cast<QuickSettingsSlider*>(mic_gain_slider_view->slider())
                ->slider_style(),
            QuickSettingsSlider::Style::kRadioActive);
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       DualInternalMicSliderInactiveState) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kFrontMic, kRearMic, kMicJack}));

  // Verify the device has dual internal mics.
  EXPECT_TRUE(cras_audio_handler_->HasDualInternalMic());

  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();

  // Verify there are 2 sliders in the view and one of them is the new dual
  // internal mic.
  EXPECT_EQ(input_sliders_map_.size(), 2u);
  EXPECT_TRUE(input_sliders_map_.contains(kDualInternalMicId));

  // Sets different volume gain levels for front and rear mics.
  cras_audio_handler_->SetVolumeGainPercentForDevice(kFrontMicId,
                                                     kFrontMicGainPercent);
  cras_audio_handler_->SetVolumeGainPercentForDevice(kRearMicId,
                                                     kRearMicGainPercent);

  // Switches to `kMicJack` to make the internal mic inactive.
  cras_audio_handler_->SwitchToDevice(AudioDevice(GenerateAudioNode(kMicJack)),
                                      true,
                                      DeviceActivateType::kActivateByUser);

  // Verifies the dual internal mic slider is inactive and its volume level
  // equals to the front mic's level.
  auto* mic_gain_slider_view = static_cast<MicGainSliderView*>(
      input_sliders_map_.find(kDualInternalMicId)->second);
  EXPECT_EQ(static_cast<QuickSettingsSlider*>(mic_gain_slider_view->slider())
                ->slider_style(),
            QuickSettingsSlider::Style::kRadioInactive);
  EXPECT_EQ(mic_gain_slider_view->slider()->GetValue(),
            kFrontMicGainPercent / 100.0);
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationToggleNotDisplayedIfNotSupported) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(false);

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      DeviceActivateType::kActivateByUser);

  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();
  EXPECT_EQ(0u, toggles_map_.size());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationToggleDisplayedIfSupportedAndInternal) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler_->RequestNoiseCancellationSupported(base::DoNothing());

  auto internal_mic = AudioDevice(GenerateAudioNode(kInternalMic));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      DeviceActivateType::kActivateByUser);

  // If `audio_detailed_view_` doesn't exist, this getter method will create the
  // view first.
  GetAudioDetailedView();
  EXPECT_EQ(1u, toggles_map_.size());
  noise_cancellation_button()->GetIsOn();
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationToggleChangesPrefAndSendsDbusSignal) {
  audio_pref_handler_->SetNoiseCancellationState(false);

  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler_->RequestNoiseCancellationSupported(base::DoNothing());

  auto internal_mic = AudioDevice(GenerateAudioNode(kInternalMic));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      DeviceActivateType::kActivateByUser);

  // If `audio_detailed_view_` doesn't exist, this getter method will create the
  // view first.
  GetAudioDetailedView();
  EXPECT_EQ(1u, toggles_map_.size());

  views::ToggleButton* toggle = noise_cancellation_button();
  auto widget = CreateFramelessTestWidget();
  widget->SetContentsView(toggle);

  // The toggle loaded the pref correctly.
  EXPECT_FALSE(toggle->GetIsOn());
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());

  // The entire row of `noise_cancellation_view_` is clickable.
  ToggleNoiseCancellation();
  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());

  ToggleNoiseCancellation();
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationUpdatedWhenDeviceChanges) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler_->RequestNoiseCancellationSupported(base::DoNothing());

  cras_audio_handler_->SwitchToDevice(AudioDevice(GenerateAudioNode(kMicJack)),
                                      true,
                                      DeviceActivateType::kActivateByUser);

  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();

  EXPECT_EQ(0u, toggles_map_.size());

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(1u, toggles_map_.size());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       NoiseCancellationUpdatedWhenOnNoiseCancellationChanges) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler_->RequestNoiseCancellationSupported(base::DoNothing());

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      DeviceActivateType::kActivateByUser);

  // If `audio_detailed_view_` doesn't exist, this getter method will create the
  // view first.
  GetAudioDetailedView();

  auto widget = CreateFramelessTestWidget();
  widget->SetContentsView(noise_cancellation_button());

  // The noise cancellation button loaded the pref correctly.
  EXPECT_TRUE(noise_cancellation_button()->GetIsOn());
  EXPECT_TRUE(audio_pref_handler_->GetNoiseCancellationState());

  cras_audio_handler_->SetNoiseCancellationState(
      /*noise_cancellation_on=*/false,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray);

  // The noise cancellation button updates the pref correctly.
  EXPECT_FALSE(noise_cancellation_button()->GetIsOn());
  EXPECT_FALSE(audio_pref_handler_->GetNoiseCancellationState());
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kSystemTray, 1);
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       StyleTransferToggleNotDisplayedIfNotSupported) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList(
          {kInternalMicStyleTransfer, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetStyleTransferSupported(false);

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMicStyleTransfer)), true,
      DeviceActivateType::kActivateByUser);

  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();
  EXPECT_EQ(0u, style_transfer_toggles_map_.size());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       StyleTransferToggleDisplayedIfSupportedAndInternal) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList(
          {kInternalMicStyleTransfer, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler_->RequestStyleTransferSupported(base::DoNothing());

  auto internal_mic =
      AudioDevice(GenerateAudioNode(kInternalMicStyleTransfer));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      DeviceActivateType::kActivateByUser);

  // If `audio_detailed_view_` doesn't exist, this getter method will create the
  // view first.
  GetAudioDetailedView();
  EXPECT_EQ(1u, style_transfer_toggles_map_.size());
  style_transfer_button()->GetIsOn();
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       StyleTransferToggleChangesPrefAndSendsDbusSignal) {
  audio_pref_handler_->SetStyleTransferState(false);

  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList(
          {kInternalMicStyleTransfer, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler_->RequestStyleTransferSupported(base::DoNothing());

  auto internal_mic =
      AudioDevice(GenerateAudioNode(kInternalMicStyleTransfer));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      DeviceActivateType::kActivateByUser);

  // If `audio_detailed_view_` doesn't exist, this getter method will create the
  // view first.
  GetAudioDetailedView();
  EXPECT_EQ(1u, style_transfer_toggles_map_.size());

  views::ToggleButton* toggle = style_transfer_button();
  auto widget = CreateFramelessTestWidget();
  widget->SetContentsView(toggle);

  // The toggle loaded the pref correctly.
  EXPECT_FALSE(toggle->GetIsOn());
  EXPECT_FALSE(audio_pref_handler_->GetStyleTransferState());

  // The entire row of `style_transfer_view_` is clickable.
  ToggleStyleTransfer();
  EXPECT_TRUE(audio_pref_handler_->GetStyleTransferState());

  ToggleStyleTransfer();
  EXPECT_FALSE(audio_pref_handler_->GetStyleTransferState());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       StyleTransferUpdatedWhenDeviceChanges) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList(
          {kInternalMicStyleTransfer, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler_->RequestStyleTransferSupported(base::DoNothing());

  cras_audio_handler_->SwitchToDevice(AudioDevice(GenerateAudioNode(kMicJack)),
                                      true,
                                      DeviceActivateType::kActivateByUser);

  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();

  EXPECT_EQ(0u, style_transfer_toggles_map_.size());

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMicStyleTransfer)), true,
      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(1u, style_transfer_toggles_map_.size());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       StyleTransferUpdatedWhenOnStyleTransferChanges) {
  audio_pref_handler_->SetStyleTransferState(true);

  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList(
          {kInternalMicStyleTransfer, kMicJack, kFrontMic, kRearMic}));
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler_->RequestStyleTransferSupported(base::DoNothing());

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMicStyleTransfer)), true,
      DeviceActivateType::kActivateByUser);

  // If `audio_detailed_view_` doesn't exist, this getter method will create the
  // view first.
  GetAudioDetailedView();

  auto widget = CreateFramelessTestWidget();
  widget->SetContentsView(style_transfer_button());

  // The style transfer button loaded the pref correctly.
  EXPECT_TRUE(style_transfer_button()->GetIsOn());
  EXPECT_TRUE(audio_pref_handler_->GetStyleTransferState());

  cras_audio_handler_->SetStyleTransferState(/*style_transfer_on=*/false);

  // The style transfer button updates the pref correctly.
  EXPECT_FALSE(style_transfer_button()->GetIsOn());
  EXPECT_FALSE(audio_pref_handler_->GetStyleTransferState());
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       StyleTransferViewHasFocusWhenPressed) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMicStyleTransfer}));
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler_->RequestStyleTransferSupported(base::DoNothing());

  auto internal_mic = AudioDevice(GenerateAudioNode(kInternalMicStyleTransfer));
  cras_audio_handler_->SwitchToDevice(internal_mic, true,
                                      DeviceActivateType::kActivateByUser);

  // A widget is required to make `HasFocus` take effect.
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();
  std::unique_ptr<views::View> audio_detailed_view =
      audio_detailed_view_controller_->CreateView();
  widget->SetContentsView(audio_detailed_view.get());

  ASSERT_EQ(1u, style_transfer_toggles_map_.size());
  EXPECT_FALSE(style_transfer_toggles_map_.begin()->second->HasFocus());

  ToggleStyleTransfer(
      static_cast<AudioDetailedView*>(audio_detailed_view.get()));

  ASSERT_EQ(1u, style_transfer_toggles_map_.size());
  EXPECT_TRUE(style_transfer_toggles_map_.begin()->second->HasFocus());
}

TEST_F(UnifiedAudioDetailedViewControllerTest, ToggleLiveCaption) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      {ash::features::kOnDeviceSpeechRecognition}, {});

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
  // not be visible in audio settings.
  EXPECT_FALSE(live_caption_view());
  EXPECT_FALSE(live_caption_enabled());
}

TEST_F(UnifiedAudioDetailedViewControllerTest, SliderFocusToggleMute) {
  auto widget = CreateFramelessTestWidget();
  widget->SetContentsView(audio_detailed_view_controller_->CreateView());
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic, kInternalSpeaker}));

  // Sets the level to make sure the slider's volume is not 0. Otherwise the
  // slider is still muted even if it's toggled on.
  const int gain = 80;
  cras_audio_handler_->SetVolumeGainPercentForDevice(kInternalMicId, gain);
  cras_audio_handler_->SetVolumeGainPercentForDevice(kInternalSpeakerId, gain);

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic)), true,
      DeviceActivateType::kActivateByUser);
  CheckSliderFocusBehavior(/*is_input_slider=*/true, kInternalMicId);

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalSpeaker)), true,
      DeviceActivateType::kActivateByUser);
  CheckSliderFocusBehavior(/*is_input_slider=*/false, kInternalSpeakerId);
}

TEST_F(UnifiedAudioDetailedViewControllerTest,
       BluetoothOutputDeviceVolumeChange) {
  std::unique_ptr<views::View> view =
      audio_detailed_view_controller_->CreateView();
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kHeadphone}));

  cras_audio_handler_->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kHeadphone)), true,
      DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kHeadphoneId, cras_audio_handler_->GetPrimaryActiveOutputNode());

  // Sets a volume level greater than 0 and the device is unmuted.
  cras_audio_handler_->SetVolumeGainPercentForDevice(kHeadphoneId,
                                                     /*value=*/10);
  EXPECT_FALSE(cras_audio_handler_->IsOutputMutedForDevice(kHeadphoneId));

  // Sets the device level to 0. The mute state is controlled by the mute
  // button so the output is still unmuted.
  cras_audio_handler_->SetVolumeGainPercentForDevice(kHeadphoneId, /*value=*/0);
  EXPECT_FALSE(cras_audio_handler_->IsOutputMutedForDevice(kHeadphoneId));
}

class UnifiedAudioDetailedViewControllerSodaTest
    : public UnifiedAudioDetailedViewControllerTest {
 protected:
  UnifiedAudioDetailedViewControllerSodaTest() = default;
  UnifiedAudioDetailedViewControllerSodaTest(
      const UnifiedAudioDetailedViewControllerSodaTest&) = delete;
  UnifiedAudioDetailedViewControllerSodaTest& operator=(
      const UnifiedAudioDetailedViewControllerSodaTest&) = delete;
  ~UnifiedAudioDetailedViewControllerSodaTest() override = default;

  void SetUp() override {
    UnifiedAudioDetailedViewControllerTest::SetUp();
    // Since this test suite is part of ash unit tests, the
    // SodaInstallerImplChromeOS is never created (it's normally created when
    // `ChromeBrowserMainPartsAsh` initializes). Create it here so that
    // calling speech::SodaInstaller::GetInstance() returns a valid instance.
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kOnDeviceSpeechRecognition,
         media::kLiveCaptionMultiLanguage},
        {});
    soda_installer_impl_ =
        std::make_unique<speech::SodaInstallerImplChromeOS>();

    EnableLiveCaption(true);
    SetLiveCaptionViewSubtitleText(kInitialLiveCaptionViewSubtitleText);
    SetLiveCaptionLocale("en-US");
  }

  void TearDown() override {
    soda_installer_impl_.reset();
    UnifiedAudioDetailedViewControllerTest::TearDown();
  }

  void EnableLiveCaption(bool enabled) {
    Shell::Get()->accessibility_controller()->live_caption().SetEnabled(
        enabled);
  }

  void SetLiveCaptionLocale(const std::string& locale) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetString(
        ::prefs::kLiveCaptionLanguageCode, locale);
  }

  speech::SodaInstaller* soda_installer() {
    return speech::SodaInstaller::GetInstance();
  }

  void SetLiveCaptionViewSubtitleText(std::u16string text) {
    live_caption_view()->SetSubText(text);
  }

  std::u16string GetLiveCaptionViewSubtitleText() {
    return live_caption_view()->sub_text_label()->GetText();
  }

 private:
  std::unique_ptr<speech::SodaInstallerImplChromeOS> soda_installer_impl_;
};

// Ensures that the Dictation subtitle changes when SODA AND the language pack
// matching the Live Caption locale are installed.
TEST_F(UnifiedAudioDetailedViewControllerSodaTest,
       OnSodaInstalledNotification) {
  SetLiveCaptionLocale("fr-FR");

  // Pretend that the SODA binary was installed. We still need to wait for the
  // correct language pack before doing anything.
  soda_installer()->NotifySodaInstalledForTesting();
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting(fr_fr());
  EXPECT_EQ(kSodaDownloaded, GetLiveCaptionViewSubtitleText());
}

// Ensures we only notify the user of progress for the language pack matching
// the Live Caption locale.
TEST_F(UnifiedAudioDetailedViewControllerSodaTest, OnSodaProgressNotification) {
  SetLiveCaptionLocale("en-US");

  soda_installer()->NotifySodaProgressForTesting(75, fr_fr());
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(50);
  EXPECT_EQ(kSodaInProgress50, GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(25, en_us());
  EXPECT_EQ(kSodaInProgress25, GetLiveCaptionViewSubtitleText());
}

// Ensures we notify the user of an error when the SODA binary fails to
// download.
TEST_F(UnifiedAudioDetailedViewControllerSodaTest,
       SodaBinaryErrorNotification) {
  soda_installer()->NotifySodaErrorForTesting();
  EXPECT_EQ(kSodaFailed, GetLiveCaptionViewSubtitleText());
}

// Ensures we only notify the user of an error if the failed language pack
// matches the Live Caption locale.
TEST_F(UnifiedAudioDetailedViewControllerSodaTest,
       SodaLanguageErrorNotification) {
  SetLiveCaptionLocale("en-US");
  soda_installer()->NotifySodaErrorForTesting(fr_fr());
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaErrorForTesting(en_us());
  EXPECT_EQ(kSodaFailed, GetLiveCaptionViewSubtitleText());
}

// Ensures that we don't respond to SODA download updates when Live Caption is
// off.
TEST_F(UnifiedAudioDetailedViewControllerSodaTest,
       SodaDownloadLiveCaptionDisabled) {
  EnableLiveCaption(false);
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaErrorForTesting();
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting();
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(50);
  EXPECT_EQ(kInitialLiveCaptionViewSubtitleText,
            GetLiveCaptionViewSubtitleText());
}

}  // namespace ash
