// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/compat_mode_button_controller.h"

#include <memory>
#include <string>

#include "ash/components/arc/compat_mode/resize_util.h"
#include "ash/components/arc/compat_mode/test/compat_mode_test_base.h"
#include "ash/public/cpp/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_center_button.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace arc {

namespace {

class TestCompatModeButtonController : public CompatModeButtonController {
 public:
  ~TestCompatModeButtonController() override = default;

  // CompatModeButtonController:
  chromeos::FrameHeader* GetFrameHeader(aura::Window* window) override {
    return frame_header_.get();
  }

  void set_frame_header(std::unique_ptr<chromeos::FrameHeader> frame_header) {
    frame_header_ = std::move(frame_header);
  }

 private:
  std::unique_ptr<chromeos::FrameHeader> frame_header_;
};

class FakeFrameHeader : public chromeos::FrameHeader {
 public:
  explicit FakeFrameHeader(views::Widget* widget)
      : chromeos::FrameHeader(widget, widget->non_client_view()->frame_view()) {
  }
  ~FakeFrameHeader() override = default;

  // FrameHeader:
  void UpdateFrameColors() override {}

 protected:
  // FrameHeader:
  void DoPaintHeader(gfx::Canvas* canvas) override {}
  views::CaptionButtonLayoutSize GetButtonLayoutSize() const override {
    return views::CaptionButtonLayoutSize::kBrowserCaptionMaximized;
  }
  SkColor GetTitleColor() const override { return gfx::kPlaceholderColor; }
  SkColor GetCurrentFrameColor() const override {
    return gfx::kPlaceholderColor;
  }
};

}  // namespace

class CompatModeButtonControllerTest : public CompatModeTestBase {
 public:
  // CompatModeTestBase:
  void SetUp() override {
    CompatModeTestBase::SetUp();
    widget_ = CreateArcWidget(/*app_id=*/std::nullopt);
    controller_.set_frame_header(
        std::make_unique<FakeFrameHeader>(widget_.get()));
    controller_.SetPrefDelegate(pref_delegate());
  }
  void TearDown() override {
    widget_->CloseNow();
    CompatModeTestBase::TearDown();
  }

  TestCompatModeButtonController* controller() { return &controller_; }
  views::Widget* widget() { return widget_.get(); }
  aura::Window* window() { return widget()->GetNativeWindow(); }

 private:
  std::unique_ptr<views::Widget> widget_;
  TestCompatModeButtonController controller_;
};

TEST_F(CompatModeButtonControllerTest, ConstructDestruct) {}

TEST_F(CompatModeButtonControllerTest, UpdateWithoutAppId) {
  const auto* frame_header = controller()->GetFrameHeader(window());

  controller()->Update(window());
  EXPECT_FALSE(frame_header->GetCenterButton());
}

TEST_F(CompatModeButtonControllerTest, UpdateWithStateUndefined) {
  const std::string app_id = "app_id";
  window()->SetProperty(ash::kAppIDKey, app_id);

  const auto* frame_header = controller()->GetFrameHeader(window());

  pref_delegate()->SetResizeLockState(app_id,
                                      mojom::ArcResizeLockState::UNDEFINED);
  SyncResizeLockPropertyWithMojoState(widget());
  controller()->Update(window());
  EXPECT_FALSE(frame_header->GetCenterButton());
}

TEST_F(CompatModeButtonControllerTest, UpdateWithStateReady) {
  const std::string app_id = "app_id";
  window()->SetProperty(ash::kAppIDKey, app_id);

  const auto* frame_header = controller()->GetFrameHeader(window());

  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::READY);
  SyncResizeLockPropertyWithMojoState(widget());
  controller()->Update(window());
  EXPECT_FALSE(frame_header->GetCenterButton());
}

TEST_F(CompatModeButtonControllerTest, UpdateWithStateOn) {
  const std::string app_id = "app_id";
  window()->SetProperty(ash::kAppIDKey, app_id);

  const auto* frame_header = controller()->GetFrameHeader(window());

  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::ON);
  SyncResizeLockPropertyWithMojoState(widget());
  // Phone
  ResizeLockToPhone(widget(), pref_delegate());
  controller()->Update(window());
  EXPECT_TRUE(frame_header->GetCenterButton());
  EXPECT_TRUE(frame_header->GetCenterButton()->GetEnabled());
  // Tablet
  ResizeLockToTablet(widget(), pref_delegate());
  controller()->Update(window());
  EXPECT_TRUE(frame_header->GetCenterButton());
  EXPECT_TRUE(frame_header->GetCenterButton()->GetEnabled());
}

TEST_F(CompatModeButtonControllerTest, UpdateWithStateOff) {
  const std::string app_id = "app_id";
  window()->SetProperty(ash::kAppIDKey, app_id);

  const auto* frame_header = controller()->GetFrameHeader(window());

  pref_delegate()->SetResizeLockState(app_id, mojom::ArcResizeLockState::OFF);
  SyncResizeLockPropertyWithMojoState(widget());
  controller()->Update(window());
  EXPECT_TRUE(frame_header->GetCenterButton());
  EXPECT_TRUE(frame_header->GetCenterButton()->GetEnabled());
}

TEST_F(CompatModeButtonControllerTest, UpdateWithStateFullyLocked) {
  const std::string app_id = "app_id";
  window()->SetProperty(ash::kAppIDKey, app_id);

  const auto* frame_header = controller()->GetFrameHeader(window());

  pref_delegate()->SetResizeLockState(app_id,
                                      mojom::ArcResizeLockState::FULLY_LOCKED);
  widget()->widget_delegate()->SetCanResize(false);
  SyncResizeLockPropertyWithMojoState(widget());
  // Phone
  ResizeLockToPhone(widget(), pref_delegate());
  controller()->Update(window());
  EXPECT_TRUE(frame_header->GetCenterButton());
  EXPECT_FALSE(frame_header->GetCenterButton()->GetEnabled());
  // Tablet
  ResizeLockToTablet(widget(), pref_delegate());
  controller()->Update(window());
  EXPECT_TRUE(frame_header->GetCenterButton());
  EXPECT_FALSE(frame_header->GetCenterButton()->GetEnabled());
}

}  // namespace arc
