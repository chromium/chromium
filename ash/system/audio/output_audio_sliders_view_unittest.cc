// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/output_audio_sliders_view.h"

#include <memory>
#include <vector>

#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr uint64_t kInternalSpeakerId = 10001;
constexpr uint64_t kHeadphoneId = 10002;

struct AudioNodeInfo {
  constexpr AudioNodeInfo(const uint64_t id,
                          const char* const device_name,
                          const char* const type,
                          const char* const name)
      : id(id), device_name(device_name), type(type), name(name) {}
  const uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

constexpr AudioNodeInfo kInternalSpeaker(kInternalSpeakerId,
                                         /*device_name*/ "Fake Speaker",
                                         /*type*/ "INTERNAL_SPEAKER",
                                         /*name*/ "Speaker");
constexpr AudioNodeInfo kHeadphone(kHeadphoneId,
                                   /*device_name*/ "Fake Headphone",
                                   /*type*/ "HEADPHONE",
                                   /*name*/ "Headphone");

AudioNode GenerateAudioNode(const AudioNodeInfo& node_info) {
  return AudioNode(/*is_input=*/false, node_info.id,
                   /*has_v2_stable_device_id=*/false,
                   /*stable_device_id_v1=*/node_info.id,
                   /*stable_device_id_v2=*/0, node_info.device_name,
                   node_info.type, node_info.name,
                   /*is_active=*/false, /*pluged_time=*/0,
                   /*max_supported_channels=*/2,
                   /*audio_effect=*/0,
                   /*number_of_volume_steps=*/25);
}

AudioNodeList GenerateAudioNodeList(
    const std::vector<AudioNodeInfo>& node_infos) {
  AudioNodeList node_list(node_infos.size());
  base::ranges::transform(node_infos, node_list.begin(),
                          [](const AudioNodeInfo& node_info) {
                            return GenerateAudioNode(node_info);
                          });
  return node_list;
}
}  // namespace

class OutputAudioSlidersViewTest : public AshTestBase {
 public:
  // Mock callback:
  MOCK_METHOD(void, OnDeviceListUpdated, (const bool has_devices), ());

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kBackgroundListening);
    AshTestBase::SetUp();
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  void CreateView() {
    view_ = widget_->SetContentsView(std::make_unique<OutputAudioSlidersView>(
        base::BindRepeating(&OutputAudioSlidersViewTest::OnDeviceListUpdated,
                            base::Unretained(this))));
  }

  std::vector<raw_ptr<views::View, VectorExperimental>>
  GetContainerChildViews() {
    return view_->GetSliderContainerForTesting()->children();
  }

  HoverHighlightView* FindView(uint64_t device_id) {
    auto device_map = view_->GetMapForTesting();
    // Iterates the `output_devices_by_name_views_` to find the corresponding
    // view.
    auto it = base::ranges::find(
        device_map, device_id, [](const AudioDeviceViewMap::value_type& value) {
          return value.second.id;
        });

    return it == device_map.end()
               ? nullptr
               : views::AsViewClass<HoverHighlightView>(it->first);
  }

  // The entries may not be rendered immediately due to the async layout. Here
  // we manually layout to make sure the views are rendered.
  void LayoutEntriesIfNecessary() {
    view_->GetWidget()->LayoutRootViewIfNecessary();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<OutputAudioSlidersView> view_ = nullptr;
};

TEST_F(OutputAudioSlidersViewTest, RendersSliderCorrectly) {
  CreateView();
  FakeCrasAudioClient::Get()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone}));

  CrasAudioHandler::Get()->SwitchToDevice(
      /*device=*/AudioDevice(GenerateAudioNode(kInternalSpeaker)),
      /*notify=*/true,
      /*activate_by*/ DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kInternalSpeakerId,
            CrasAudioHandler::Get()->GetPrimaryActiveOutputNode());

  // Both sliders should be visible and rendered correctly.
  HoverHighlightView* internal_speaker_slider = FindView(kInternalSpeakerId);
  ASSERT_TRUE(internal_speaker_slider);
  EXPECT_TRUE(internal_speaker_slider->GetVisible());
  EXPECT_EQ(internal_speaker_slider->text_label()->GetText(),
            u"Speaker (internal)");
  HoverHighlightView* headphone_slider = FindView(kHeadphoneId);
  ASSERT_TRUE(headphone_slider);
  EXPECT_TRUE(headphone_slider->GetVisible());
  EXPECT_EQ(headphone_slider->text_label()->GetText(), u"Headphones");

  CrasAudioHandler::Get()->SwitchToDevice(
      /*device=*/AudioDevice(GenerateAudioNode(kHeadphone)),
      /*notify=*/true,
      /*activate_by*/ DeviceActivateType::kActivateByUser);
  EXPECT_EQ(kHeadphoneId,
            CrasAudioHandler::Get()->GetPrimaryActiveOutputNode());

  // Both sliders should be visible and rendered correctly.
  internal_speaker_slider = FindView(kInternalSpeakerId);
  ASSERT_TRUE(internal_speaker_slider);
  EXPECT_TRUE(internal_speaker_slider->GetVisible());
  EXPECT_EQ(u"Speaker (internal)",
            internal_speaker_slider->text_label()->GetText());
  headphone_slider = FindView(kHeadphoneId);
  ASSERT_TRUE(headphone_slider);
  EXPECT_TRUE(headphone_slider->GetVisible());
  EXPECT_EQ(u"Headphones", headphone_slider->text_label()->GetText());
}

TEST_F(OutputAudioSlidersViewTest, UpdateDevices) {
  // Updates with default devices.
  EXPECT_CALL(*this, OnDeviceListUpdated);
  CreateView();
  testing::Mock::VerifyAndClearExpectations(this);

  // There's no empty device list cases in the audio handler. So we skip the 0
  // device case.

  // Updates with 2 devices.
  EXPECT_CALL(*this, OnDeviceListUpdated).Times(2);
  FakeCrasAudioClient::Get()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalSpeaker, kHeadphone}));
  LayoutEntriesIfNecessary();
  EXPECT_EQ(GetContainerChildViews().size(), 2u);
  testing::Mock::VerifyAndClearExpectations(this);

  // Updates with 1 device.
  EXPECT_CALL(*this, OnDeviceListUpdated).Times(2);
  FakeCrasAudioClient::Get()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalSpeaker}));
  LayoutEntriesIfNecessary();
  EXPECT_EQ(GetContainerChildViews().size(), 1u);
  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace ash
