// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/tray_cast.h"

#include <memory>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "ui/views/view.h"
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
  void StopCasting(const std::string& route_id) override {}

  std::vector<SinkAndRoute> sinks_and_routes_;
  size_t cast_to_sink_count_ = 0;
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

  // Device views are children of the rounded container.
  for (views::View* view : GetDeviceViews()) {
    EXPECT_STREQ(view->parent()->GetClassName(), "RoundedContainer");
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

}  // namespace ash
