// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_untrusted_page_handler.h"

#include "ash/test/ash_test_base.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_delegate.h"
#include "ash/webui/demo_mode_app_ui/mojom/demo_mode_app_untrusted_ui.mojom.h"
#include "ash/wm/container_finder.h"
#include "base/test/run_until.h"
#include "chromeos/ash/components/test/ash_test_suite.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {

class MockAppDelegate : public DemoModeAppDelegate {
 public:
  // DemoModeAppDelegate:
  MOCK_METHOD1(LaunchApp, void(const std::string&));
  MOCK_METHOD0(RemoveSplashScreen, void());
};

class DemoModeUntrustedPageHandlerTest : public AshTestBase {
 public:
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    widget_ = AshTestBase::CreateTestWidget(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    // Demo app init with a non-fullscreen.
    widget_->SetFullscreen(false);

    handler_ = std::make_unique<DemoModeUntrustedPageHandler>(
        handler_remote_.BindNewPipeAndPassReceiver(), widget_.get(),
        &app_delegate_);
  }

 protected:
  const mojo::Remote<mojom::demo_mode::UntrustedPageHandler>& handler_remote() {
    return handler_remote_;
  }

  std::unique_ptr<views::Widget> widget_;
  MockAppDelegate app_delegate_;

 private:
  mojo::Remote<mojom::demo_mode::UntrustedPageHandler> handler_remote_;
  std::unique_ptr<DemoModeUntrustedPageHandler> handler_;
};

TEST_F(DemoModeUntrustedPageHandlerTest, ToggleFullscreen) {
  EXPECT_CALL(app_delegate_, RemoveSplashScreen()).Times(2);
  handler_remote()->ToggleFullscreen();
  EXPECT_TRUE(base::test::RunUntil([&]() { return widget_->IsFullscreen(); }));
  EXPECT_EQ(GetContainerForWindow(widget_->GetNativeWindow())->GetId(),
            kShellWindowId_AlwaysOnTopWallpaperContainer);

  handler_remote()->ToggleFullscreen();
  EXPECT_TRUE(base::test::RunUntil([&]() { return !widget_->IsFullscreen(); }));
  EXPECT_EQ(GetContainerForWindow(widget_->GetNativeWindow())->GetId(),
            desks_util::GetActiveDeskContainerId());
}

}  // namespace ash
