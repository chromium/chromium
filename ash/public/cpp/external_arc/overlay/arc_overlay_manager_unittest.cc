// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"

#include "ash/test/test_widget_builder.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/shell_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"

namespace ash {
namespace {

constexpr char kOverlayToken[] = "overlay_token";
constexpr char kOverlayClientSurfaceId[] = "billing_id:overlay_token";

class ArcOverlayManagerTest : public exo::test::ExoTestBase {
 public:
  void SetUp() override {
    exo::test::ExoTestBase::SetUp();

    manager_ = std::make_unique<ArcOverlayManager>();

    host_widget_ = TestWidgetBuilder().BuildOwnsNativeWidget();

    exo::test::ShellSurfaceBuilder builder(gfx::Size(100, 100));
    overlay_shell_surface_ = builder.BuildShellSurface();
    overlay_window_ = overlay_shell_surface_->GetWidget()->GetNativeWindow();
    overlay_shell_surface_->root_surface()->SetClientSurfaceId(
        kOverlayClientSurfaceId);
    overlay_window_->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::ARC_APP);
    manager_->OnWindowInitialized(overlay_window_);
    deregister_closure_ = manager_->RegisterHostWindow(
        kOverlayToken, host_widget_->GetNativeWindow());
  }

  void TearDown() override {
    deregister_closure_.RunAndReset();
    overlay_shell_surface_.reset();
    manager_.reset();

    exo::test::ExoTestBase::TearDown();
  }

  void MakeOverlayWindowVisible() {
    manager_->OnWindowVisibilityChanged(overlay_window_, true);
  }

  void CloseHostWindow() { host_widget_.reset(); }

  void CloseOverlayWindow() { deregister_closure_.RunAndReset(); }

  aura::Window* host_window() { return host_widget_->GetNativeWindow(); }
  aura::Window* overlay_window() { return overlay_window_; }

 private:
  std::unique_ptr<ArcOverlayManager> manager_;
  std::unique_ptr<views::Widget> host_widget_;

  std::unique_ptr<exo::ShellSurface> overlay_shell_surface_;
  raw_ptr<aura::Window, DanglingUntriaged> overlay_window_ = nullptr;

  base::ScopedClosureRunner deregister_closure_;
};

TEST_F(ArcOverlayManagerTest, SkipImeProcessingProperty) {
  EXPECT_FALSE(overlay_window()->GetProperty(aura::client::kSkipImeProcessing));
  MakeOverlayWindowVisible();
  EXPECT_TRUE(overlay_window()->GetProperty(aura::client::kSkipImeProcessing));
}

TEST_F(ArcOverlayManagerTest,
       CanConsumeSystemKeysSetToFalseWhileOverlayIsActive) {
  ash::WindowState* host_window_state = ash::WindowState::Get(host_window());
  host_window_state->SetCanConsumeSystemKeys(true);

  EXPECT_TRUE(host_window_state->CanConsumeSystemKeys());
  MakeOverlayWindowVisible();
  EXPECT_FALSE(host_window_state->CanConsumeSystemKeys());
  CloseOverlayWindow();
  EXPECT_TRUE(host_window_state->CanConsumeSystemKeys());
}

TEST_F(ArcOverlayManagerTest, CanConsumeSystemKeysRestoredToFalseAfterOverlay) {
  ash::WindowState* host_window_state = ash::WindowState::Get(host_window());
  host_window_state->SetCanConsumeSystemKeys(false);

  EXPECT_FALSE(host_window_state->CanConsumeSystemKeys());
  MakeOverlayWindowVisible();
  EXPECT_FALSE(host_window_state->CanConsumeSystemKeys());
  CloseOverlayWindow();
  EXPECT_FALSE(host_window_state->CanConsumeSystemKeys());
}

TEST_F(ArcOverlayManagerTest, CanCloseHostWindowWhileOverlayIsActive) {
  MakeOverlayWindowVisible();
  CloseHostWindow();
}

}  // namespace
}  // namespace ash
