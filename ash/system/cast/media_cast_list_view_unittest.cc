// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/media_cast_list_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/style/pill_button.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class MediaCastListViewTest : public AshTestBase {
 public:
  // Mock callbacks:
  MOCK_METHOD(void, OnStopCasting, (), ());
  MOCK_METHOD(void, OnCastDeviceSelected, (const std::string& device_id), ());
  MOCK_METHOD(void, OnDeviceListUpdated, (const bool has_devices), ());

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(media::kBackgroundListening);
    AshTestBase::SetUp();
    // Create a widget so tests can click on views.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    view_ = widget_->SetContentsView(std::make_unique<MediaCastListView>(
        base::BindRepeating(&MediaCastListViewTest::OnStopCasting,
                            base::Unretained(this)),
        base::BindRepeating(&MediaCastListViewTest::OnCastDeviceSelected,
                            base::Unretained(this)),
        base::BindRepeating(&MediaCastListViewTest::OnDeviceListUpdated,
                            base::Unretained(this)),
        client_remote_.BindNewPipeAndPassReceiver()));
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    AshTestBase::TearDown();
  }

  MediaCastListView* media_cast_list_view() { return view_; }
  std::vector<raw_ptr<views::View, VectorExperimental>>
  GetContainerChildViews() {
    return view_->item_container_->children();
  }

  views::View* GetStopCastingButton() {
    return view_->GetViewByID(kStopCastingButtonId);
  }

  // Adds 3 simulated cast devices.
  void AddCastDevices() {
    std::vector<global_media_controls::mojom::DevicePtr> devices;
    global_media_controls::mojom::DevicePtr device1 =
        global_media_controls::mojom::Device::New(
            /*id=*/"fake_sink_id_1",
            /*name=*/"Sink Name 1",
            /*status_text=*/"",
            /*icon=*/global_media_controls::mojom::IconType::kTv);
    devices.push_back(std::move(device1));
    global_media_controls::mojom::DevicePtr device2 =
        global_media_controls::mojom::Device::New(
            /*id=*/"fake_sink_id_2",
            /*name=*/"Sink Name 2",
            /*status_text=*/"",
            /*icon=*/global_media_controls::mojom::IconType::kSpeaker);
    devices.push_back(std::move(device2));
    global_media_controls::mojom::DevicePtr device3 =
        global_media_controls::mojom::Device::New(
            /*id=*/"fake_sink_id_3",
            /*name=*/"Sink Name 3",
            /*status_text=*/"",
            /*icon=*/global_media_controls::mojom::IconType::kSpeakerGroup);
    devices.push_back(std::move(device3));
    view_->OnDevicesUpdated(std::move(devices));
  }

  // Removes simulated cast devices.
  void ResetCastDevices() { view_->OnDevicesUpdated({}); }

  // The entries may not be rendered immediately due to the async call of
  // `InvalidateLayout`. Here we manually call `layout` to make sure the views
  // are rendered.
  void LayoutEntriesIfNecessary() {
    view_->GetWidget()->LayoutRootViewIfNecessary();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<MediaCastListView> view_ = nullptr;
  bool stop_casting_ = false;
  bool start_casting_ = false;
  bool update_devices_callback_ = false;
  bool is_updated_devices_empty_ = false;
  std::string current_device_id_;
  mojo::Remote<global_media_controls::mojom::DeviceListClient> client_remote_;
};

TEST_F(MediaCastListViewTest, ViewsCreatedForCastDevices) {
  // Adding cast devices creates views.
  AddCastDevices();
  LayoutEntriesIfNecessary();

  // One header row and 3 device row.
  ASSERT_EQ(GetContainerChildViews().size(), 4u);

  HoverHighlightView* row1 =
      views::AsViewClass<HoverHighlightView>(GetContainerChildViews()[1]);
  EXPECT_EQ(row1->text_label()->GetText(), u"Sink Name 1");
  EXPECT_CALL(*this, OnCastDeviceSelected("fake_sink_id_1")).Times(1);
  LeftClickOn(row1);
  testing::Mock::VerifyAndClearExpectations(this);

  HoverHighlightView* row2 =
      static_cast<HoverHighlightView*>(GetContainerChildViews()[2]);
  EXPECT_EQ(row2->text_label()->GetText(), u"Sink Name 2");
  EXPECT_CALL(*this, OnCastDeviceSelected("fake_sink_id_2")).Times(1);
  LeftClickOn(row2);
  testing::Mock::VerifyAndClearExpectations(this);

  HoverHighlightView* row3 =
      static_cast<HoverHighlightView*>(GetContainerChildViews()[3]);
  EXPECT_EQ(row3->text_label()->GetText(), u"Sink Name 3");
  EXPECT_CALL(*this, OnCastDeviceSelected("fake_sink_id_3")).Times(1);
  LeftClickOn(row3);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(MediaCastListViewTest, WithNoDevices) {
  // Updates with an default device list.
  EXPECT_CALL(*this, OnDeviceListUpdated).Times(1);
  AddCastDevices();
  EXPECT_EQ(GetContainerChildViews().size(), 4u);
  ASSERT_TRUE(GetStopCastingButton());
  testing::Mock::VerifyAndClearExpectations(this);

  // Updates with an empty device list. Should not show `StopCastingButton`.
  EXPECT_CALL(*this, OnDeviceListUpdated).Times(1);
  ResetCastDevices();
  EXPECT_EQ(GetContainerChildViews().size(), 0u);
  EXPECT_EQ(GetStopCastingButton(), nullptr);
  testing::Mock::VerifyAndClearExpectations(this);
}

TEST_F(MediaCastListViewTest, StartAndStopCasting) {
  AddCastDevices();
  LayoutEntriesIfNecessary();

  EXPECT_FALSE(GetContainerChildViews().empty());
  views::View* first_view = GetContainerChildViews()[1];

  // Clicking on an entry triggers a cast session.
  EXPECT_CALL(*this, OnCastDeviceSelected("fake_sink_id_1")).Times(1);
  LeftClickOn(first_view);
  testing::Mock::VerifyAndClearExpectations(this);

  // Clicking on stop button triggers stop casting.
  EXPECT_CALL(*this, OnStopCasting).Times(1);
  LeftClickOn(GetStopCastingButton());
  testing::Mock::VerifyAndClearExpectations(this);
}

}  // namespace ash
