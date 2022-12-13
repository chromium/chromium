// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/tray_cast.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/style/pill_button.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class TestCastConfigController : public CastConfigController {
 public:
  TestCastConfigController() = default;
  TestCastConfigController(const TestCastConfigController&) = delete;
  TestCastConfigController& operator=(const TestCastConfigController&) = delete;
  ~TestCastConfigController() override = default;

  // CastConfigController:
  void AddObserver(Observer* observer) override {}
  void RemoveObserver(Observer* observer) override {}
  bool HasMediaRouterForPrimaryProfile() const override { return true; }
  bool HasSinksAndRoutes() const override { return false; }
  bool HasActiveRoute() const override { return false; }
  bool AccessCodeCastingEnabled() const override { return false; }
  void RequestDeviceRefresh() override {}
  const std::vector<SinkAndRoute>& GetSinksAndRoutes() override {
    return sinks_and_routes_;
  }
  void CastToSink(const std::string& sink_id) override {
    ++cast_to_sink_count_;
  }
  void StopCasting(const std::string& route_id) override {
    ++stop_casting_count_;
    stop_casting_route_id_ = route_id;
  }

  std::vector<SinkAndRoute> sinks_and_routes_;
  size_t cast_to_sink_count_ = 0;
  size_t stop_casting_count_ = 0;
  std::string stop_casting_route_id_;
};

}  // namespace

class CastDetailedViewTest : public AshTestBase {
 public:
  CastDetailedViewTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kQsRevamp, features::kQsRevampWip},
        /*disabled_features=*/{});
  }

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
    for (const auto& it : detailed_view_->view_to_sink_map_)
      views.push_back(it.first);
    return views;
  }

  views::View* GetZeroStateView() { return detailed_view_->zero_state_view_; }

  // Adds two simulated cast devices.
  void AddCastDevices() {
    std::vector<SinkAndRoute> devices;
    SinkAndRoute device1;
    device1.sink.id = "fake_sink_id_1";
    device1.sink.name = "Sink Name 1";
    device1.sink.domain = "example.com";
    device1.sink.sink_icon_type = SinkIconType::kCast;
    devices.push_back(device1);
    SinkAndRoute device2;
    device2.sink.id = "fake_sink_id_2";
    device2.sink.name = "Sink Name 2";
    device2.sink.domain = "example.com";
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

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> widget_;
  TestCastConfigController cast_config_;
  std::unique_ptr<FakeDetailedViewDelegate> delegate_;
  CastDetailedView* detailed_view_ = nullptr;
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
  EXPECT_EQ(cast_config_.cast_to_sink_count_, 1u);
  EXPECT_EQ(delegate_->close_bubble_call_count(), 1u);
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
  device.sink.domain = "example.com";
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
  EXPECT_EQ(cast_config_.stop_casting_count_, 1u);
  EXPECT_EQ(cast_config_.stop_casting_route_id_, "fake_route_id_1");
  EXPECT_EQ(delegate_->close_bubble_call_count(), 1u);
}

TEST_F(CastDetailedViewTest, NoStopCastingButtonForNonLocalSource) {
  // Set up a fake sink and a route as if some non-local source is casting to
  // the device.
  std::vector<SinkAndRoute> devices;
  SinkAndRoute device;
  device.sink.id = "fake_sink_id_1";
  device.sink.name = "Sink Name 1";
  device.sink.domain = "example.com";
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

}  // namespace ash
