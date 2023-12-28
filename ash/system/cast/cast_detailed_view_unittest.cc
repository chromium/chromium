// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_detailed_view.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/test/test_cast_config_controller.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/style/pill_button.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

class CastDetailedViewTest : public AshTestBase {
 public:
  CastDetailedViewTest() = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    // Create a widget so tests can click on views.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
    detailed_view_ = widget_->SetContentsView(
        std::make_unique<CastDetailedView>(delegate_.get()));
  }

  void TearDown() override {
    widget_.reset();
    detailed_view_ = nullptr;
    delegate_.reset();
    AshTestBase::TearDown();
  }

  std::vector<views::View*> GetDeviceViews() {
    std::vector<views::View*> views;
    for (const auto& it : detailed_view_->view_to_sink_map_) {
      views.push_back(it.first);
    }
    return views;
  }

  std::vector<raw_ptr<views::View, VectorExperimental>> GetExtraViewsForSink(
      const std::string& sink_id) {
    return detailed_view_->sink_extra_views_map_[sink_id];
  }

  views::View* GetZeroStateView() { return detailed_view_->zero_state_view_; }

  // Adds two simulated cast devices.
  void AddCastDevices() {
    std::vector<SinkAndRoute> devices;
    SinkAndRoute device1;
    device1.sink.id = "fake_sink_id_1";
    device1.sink.name = "Sink Name 1";
    device1.sink.sink_icon_type = SinkIconType::kCast;
    devices.push_back(device1);
    SinkAndRoute device2;
    device2.sink.id = "fake_sink_id_2";
    device2.sink.name = "Sink Name 2";
    device2.sink.sink_icon_type = SinkIconType::kCast;
    devices.push_back(device2);
    detailed_view_->OnDevicesUpdated(devices);
  }

  // Adds simulated cast sinks and routes.
  void OnDevicesUpdated(const std::vector<SinkAndRoute>& devices) {
    detailed_view_->OnDevicesUpdated(devices);
  }

  // Removes simulated cast devices.
  void ResetCastDevices() { detailed_view_->OnDevicesUpdated({}); }

  views::View* GetAddAccessCodeDeviceView() {
    return detailed_view_->add_access_code_device_;
  }

  std::unique_ptr<views::Widget> widget_;
  TestCastConfigController cast_config_;
  std::unique_ptr<FakeDetailedViewDelegate> delegate_;
  raw_ptr<CastDetailedView, DanglingUntriaged> detailed_view_ = nullptr;
};

TEST_F(CastDetailedViewTest, ViewsCreatedForCastDevices) {
  // Adding cast devices creates views.
  AddCastDevices();
  EXPECT_EQ(GetDeviceViews().size(), 2u);

  for (views::View* view : GetDeviceViews()) {
    // Device views are children of the rounded container.
    EXPECT_STREQ(view->parent()->GetClassName(), "RoundedContainer");

    // Device views don't have a "stop casting" button by default.
    ASSERT_TRUE(views::IsViewClass<HoverHighlightView>(view));
    HoverHighlightView* row = static_cast<HoverHighlightView*>(view);
    EXPECT_FALSE(row->right_view());
  }
}

TEST_F(CastDetailedViewTest, ClickOnViewClosesBubble) {
  AddCastDevices();
  std::vector<views::View*> views = GetDeviceViews();
  ASSERT_FALSE(views.empty());
  views::View* first_view = views[0];

  // Clicking on a view triggers a cast session and closes the bubble.
  LeftClickOn(first_view);
  EXPECT_EQ(cast_config_.cast_to_sink_count(), 1u);
  EXPECT_EQ(delegate_->close_bubble_call_count(), 1u);
}

TEST_F(CastDetailedViewTest, CastToSinkClosingBubbleDoesNotCrash) {
  AddCastDevices();
  std::vector<views::View*> views = GetDeviceViews();
  ASSERT_FALSE(views.empty());
  views::View* first_view = views[0];

  // In multi-monitor situations, casting will create a picker window to choose
  // the desktop to cast. This causes a window activation that closes the
  // system tray bubble and deletes the widget owning the CastDetailedView.
  cast_config_.set_cast_to_sink_closure(
      base::BindOnce([](CastDetailedViewTest* test) { test->widget_.reset(); },
                     base::Unretained(this)));
  LeftClickOn(first_view);
  EXPECT_EQ(cast_config_.cast_to_sink_count(), 1u);
  // No crash.
}

TEST_F(CastDetailedViewTest, AccessCodeCasting) {
  cast_config_.set_access_code_casting_enabled(true);
  ResetCastDevices();
  views::View* add_access_code_device = GetAddAccessCodeDeviceView();
  ASSERT_TRUE(add_access_code_device);

  LeftClickOn(add_access_code_device);
  EXPECT_EQ(GetSystemTrayClient()->show_access_code_casting_dialog_count(), 1);
  // The bubble is not closed via the delegate, because it happens via a focus
  // change when the dialog appears.
  EXPECT_EQ(delegate_->close_bubble_call_count(), 0u);
}

// When the screen is locked, we should not show the access code device button,
// since this opens a dialog that can't be accessed when the screen is locked.
TEST_F(CastDetailedViewTest, AccessCodeCastingButtonScreenLocked) {
  cast_config_.set_access_code_casting_enabled(true);
  GetSessionControllerClient()->LockScreen();
  ResetCastDevices();
  views::View* add_access_code_device = GetAddAccessCodeDeviceView();
  EXPECT_FALSE(add_access_code_device);
}

TEST_F(CastDetailedViewTest, ZeroStateView) {
  // The zero state view shows when there are no cast devices.
  ASSERT_TRUE(GetDeviceViews().empty());
  EXPECT_TRUE(GetZeroStateView());

  // Adding cast devices hides the zero state view.
  AddCastDevices();
  EXPECT_FALSE(GetZeroStateView());

  // Removing cast devices shows the zero state view.
  ResetCastDevices();
  EXPECT_TRUE(GetZeroStateView());
}

TEST_F(CastDetailedViewTest, StopCastingButton) {
  // Set up a fake sink and route, as if this Chromebook is casting to the
  // device.
  std::vector<SinkAndRoute> devices;
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_1";
  device.sink.name = "Sink Name 1";
  device.sink.sink_icon_type = SinkIconType::kCast;
  device.route.id = "fake_route_id_1";
  device.route.title = "Title 1";
  // Simulate a local source (this Chromebook).
  device.route.is_local_source = true;
  devices.push_back(device);
  OnDevicesUpdated(devices);

  std::vector<views::View*> views = GetDeviceViews();
  ASSERT_EQ(views.size(), 1u);
  ASSERT_TRUE(views::IsViewClass<HoverHighlightView>(views[0]));
  HoverHighlightView* row = static_cast<HoverHighlightView*>(views[0]);
  ASSERT_TRUE(row);

  // The row contains a button on the right.
  views::View* right_view = row->right_view();
  ASSERT_TRUE(right_view);
  EXPECT_TRUE(views::IsViewClass<PillButton>(right_view));
  EXPECT_EQ(right_view->GetTooltipText(gfx::Point()), u"Stop casting");

  // Clicking on the button stops casting.
  LeftClickOn(right_view);
  EXPECT_EQ(cast_config_.stop_casting_count(), 1u);
  EXPECT_EQ(cast_config_.stop_casting_route_id(), "fake_route_id_1");
  EXPECT_EQ(delegate_->close_bubble_call_count(), 1u);
}

TEST_F(CastDetailedViewTest, NoStopCastingButtonForNonLocalSource) {
  // Set up a fake sink and a route as if some non-local source is casting to
  // the device.
  std::vector<SinkAndRoute> devices;
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_1";
  device.sink.name = "Sink Name 1";
  device.sink.sink_icon_type = SinkIconType::kCast;
  device.route.id = "fake_route_id_1";
  device.route.title = "Title 1";
  // Simulate a non-local source (not this Chromebook).
  device.route.is_local_source = false;
  devices.push_back(device);
  OnDevicesUpdated(devices);

  std::vector<views::View*> views = GetDeviceViews();
  ASSERT_EQ(views.size(), 1u);
  ASSERT_TRUE(views::IsViewClass<HoverHighlightView>(views[0]));
  HoverHighlightView* row = static_cast<HoverHighlightView*>(views[0]);
  ASSERT_TRUE(row);

  // The row does not contains a right view because there is no stop casting
  // button because the cast source is not the local machine.
  EXPECT_FALSE(row->right_view());
}

TEST_F(CastDetailedViewTest, FreezeButton) {
  // Set up a fake sink and route, as if this Chromebook is casting to the
  // device. And, the route may be frozen.
  std::vector<SinkAndRoute> devices;
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_1";
  device.sink.name = "Sink Name 1";
  device.sink.sink_icon_type = SinkIconType::kCast;
  device.route.id = "fake_route_id_1";
  device.route.title = "Title 1";
  // Simulate a local source (this Chromebook).
  device.route.is_local_source = true;
  device.route.freeze_info.can_freeze = true;
  devices.push_back(device);
  OnDevicesUpdated(devices);

  std::vector<raw_ptr<views::View, VectorExperimental>> views =
      GetExtraViewsForSink("fake_sink_id_1");
  ASSERT_EQ(views.size(), 2u);
  auto* freeze_button = views[0].get();
  EXPECT_TRUE(views::IsViewClass<PillButton>(freeze_button));
  EXPECT_EQ(freeze_button->GetTooltipText(gfx::Point()), u"Pause casting");

  // Clicking on the button pauses casting.
  LeftClickOn(freeze_button);
  EXPECT_EQ(cast_config_.freeze_route_count(), 1u);
  EXPECT_EQ(cast_config_.freeze_route_route_id(), "fake_route_id_1");
  EXPECT_EQ(delegate_->close_bubble_call_count(), 1u);
}

}  // namespace ash
