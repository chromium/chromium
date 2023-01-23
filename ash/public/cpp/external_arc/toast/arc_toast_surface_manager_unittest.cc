// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/external_arc/toast/arc_toast_surface_manager.h"

#include "ash/public/cpp/session/session_controller.h"
#include "ash/test/ash_test_base.h"
#include "components/exo/surface.h"
#include "components/exo/toast_surface.h"
#include "components/exo/wm_helper.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {

class ArcToastSurfaceManagerTest : public AshTestBase {
 public:
  ArcToastSurfaceManagerTest() = default;

  ArcToastSurfaceManagerTest(const ArcToastSurfaceManagerTest&) = delete;
  ArcToastSurfaceManagerTest& operator=(const ArcToastSurfaceManagerTest&) =
      delete;

  ~ArcToastSurfaceManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    wm_helper_ = std::make_unique<exo::WMHelper>();

    // Start in the ACTIVE (logged-in) state.
    ChangeLockState(false);
  }

  void TearDown() override {
    wm_helper_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void ChangeLockState(bool lock) {
    SessionInfo info;
    info.state = lock ? session_manager::SessionState::LOCKED
                      : session_manager::SessionState::ACTIVE;
    ash::SessionController::Get()->SetSessionInfo(info);
  }

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;
};

TEST_F(ArcToastSurfaceManagerTest, AddRemoveSurface) {
  ArcToastSurfaceManager manager;
  EXPECT_TRUE(manager.toast_surfaces_.empty());
  exo::Surface surface;
  exo::ToastSurface toast_surface(nullptr, &surface,
                                  /*default_scale_cancellation=*/false);
  manager.AddSurface(&toast_surface);
  EXPECT_EQ(1U, manager.toast_surfaces_.size());
  manager.RemoveSurface(&toast_surface);
  EXPECT_TRUE(manager.toast_surfaces_.empty());
}

TEST_F(ArcToastSurfaceManagerTest, HideNewToastOnLockScreen) {
  ArcToastSurfaceManager manager;
  // Simulate device lock.
  ChangeLockState(true);

  exo::Surface surface;
  exo::ToastSurface toast_surface(&manager, &surface,
                                  /*default_scale_cancellation=*/false);
  surface.Commit();

  // Confirm that it's not visible on lock screen.
  EXPECT_FALSE(toast_surface.GetWidget()->IsVisible());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that it's back to visible after unlock.
  EXPECT_TRUE(toast_surface.GetWidget()->IsVisible());
}

TEST_F(ArcToastSurfaceManagerTest, HideExistingToastOnLockScreen) {
  ArcToastSurfaceManager manager;
  exo::Surface surface;
  exo::ToastSurface toast_surface(&manager, &surface,
                                  /*default_scale_cancellation=*/false);
  surface.Commit();

  EXPECT_TRUE(toast_surface.GetWidget()->IsVisible());

  // Simulate device lock after adding toast.
  ChangeLockState(true);
  // Confirm that it's not visible on lock screen.
  EXPECT_FALSE(toast_surface.GetWidget()->IsVisible());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that it's back to visible after unlock.
  EXPECT_TRUE(toast_surface.GetWidget()->IsVisible());
}

TEST_F(ArcToastSurfaceManagerTest, HideNewToastWhenLockedBeforeCommit) {
  ArcToastSurfaceManager manager;
  exo::Surface surface;
  exo::ToastSurface toast_surface(&manager, &surface,
                                  /*default_scale_cancellation=*/false);

  // Simulate device lock just before commit.
  ChangeLockState(true);
  surface.Commit();

  EXPECT_FALSE(toast_surface.GetWidget()->IsVisible());

  // Simulate device unlock.
  ChangeLockState(false);
  // Confirm that it's back to visible after unlock.
  EXPECT_TRUE(toast_surface.GetWidget()->IsVisible());
}

}  // namespace ash
