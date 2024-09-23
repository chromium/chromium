// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/media_cast_audio_selector_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/audio_device.h"
#include "components/global_media_controls/public/test/mock_device_service.h"
#include "components/global_media_controls/public/views/media_item_ui_view.h"
#include "components/media_message_center/mock_media_notification_item.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using global_media_controls::test::MockDeviceListHost;

namespace ash {

class MediaCastAudioSelectorViewTest : public AshTestBase {
 public:
  MediaCastAudioSelectorViewTest() = default;

  // Mock callbacks:
  MOCK_METHOD(void, OnStopCasting, (), ());

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kBackgroundListening);
    AshTestBase::SetUp();

    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    view_ =
        widget_->SetContentsView(std::make_unique<MediaCastAudioSelectorView>(
            /*device_list_host=*/device_list_host_.PassRemote(),
            /*receiver=*/client_remote_.BindNewPipeAndPassReceiver(),
            /*stop_casting_callback=*/
            base::BindRepeating(&MediaCastAudioSelectorViewTest::OnStopCasting,
                                base::Unretained(this)),
            /*show_devices=*/false));
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  MediaCastAudioSelectorView* GetSelectorView() { return view_; }

  views::View* GetListViewContainer() {
    return view_->GetViewByID(kListViewContainerId);
  }

  MediaCastListView* GetMediaCastListView() {
    return static_cast<MediaCastListView*>(
        view_->GetViewByID(kMediaCastListViewId));
  }

  std::vector<raw_ptr<views::View, VectorExperimental>>
  GetContainerChildViews() {
    return GetMediaCastListView()->item_container_->children();
  }

  // Adds 1 simulated cast device.
  void AddCastDevices() {
    std::vector<global_media_controls::mojom::DevicePtr> devices;
    global_media_controls::mojom::DevicePtr device =
        global_media_controls::mojom::Device::New(
            /*id=*/"fake_sink_id_0",
            /*name=*/"Sink Name 0",
            /*status_text=*/"",
            /*icon=*/global_media_controls::mojom::IconType::kTv);
    devices.push_back(std::move(device));
    GetMediaCastListView()->OnDevicesUpdated(std::move(devices));
  }

  void RemoveCastDevices() {
    GetMediaCastListView()->OnDevicesUpdated(/*devices=*/{});
  }

  bool GetCastDeviceBit() {
    return view_->device_type_bits_[static_cast<int>(
        MediaCastAudioSelectorView::DeviceType::kCastDevice)];
  }

  bool GetAudioDeviceBit() {
    return view_->device_type_bits_[static_cast<int>(
        MediaCastAudioSelectorView::DeviceType::kAudioDevice)];
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaCastAudioSelectorView> view_ = nullptr;
  MockDeviceListHost device_list_host_;
  mojo::Remote<global_media_controls::mojom::DeviceListClient> client_remote_;
};

TEST_F(MediaCastAudioSelectorViewTest, VisibilityChanges) {
  // Adding cast devices creates views.
  AddCastDevices();
  EXPECT_EQ(GetContainerChildViews().size(), 2u);

  // Seletor view is not visible before showing up the list views.
  EXPECT_FALSE(GetListViewContainer()->GetVisible());
  EXPECT_FALSE(GetSelectorView()->IsDeviceSelectorExpanded());

  // Seletor view  not visible after showing up the list views.
  GetSelectorView()->ShowDevices();
  EXPECT_TRUE(GetListViewContainer()->GetVisible());
  EXPECT_TRUE(GetSelectorView()->IsDeviceSelectorExpanded());

  // Seletor view is not visible after hiding the list views.
  GetSelectorView()->HideDevices();
  EXPECT_FALSE(GetListViewContainer()->GetVisible());
  EXPECT_FALSE(GetSelectorView()->IsDeviceSelectorExpanded());
}

TEST_F(MediaCastAudioSelectorViewTest, WithTwoDeviceLists) {
  // Cast devices should not exist and audio devices exist by default.
  EXPECT_FALSE(GetCastDeviceBit());
  EXPECT_TRUE(GetAudioDeviceBit());

  // Adding cast devices.
  AddCastDevices();
  EXPECT_TRUE(GetCastDeviceBit());
  EXPECT_TRUE(GetAudioDeviceBit());

  // Remove cast devices.
  RemoveCastDevices();
  EXPECT_FALSE(GetCastDeviceBit());
  EXPECT_TRUE(GetAudioDeviceBit());
}

}  // namespace ash
