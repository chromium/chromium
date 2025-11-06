// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "cc/trees/layer_tree_host.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/page_transition_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"

namespace {

void SimulateGpuCrash() {
  content::KillGpuProcess();
}

display::DisplayManager* GetDisplayManager() {
  return ash::Shell::Get()->display_manager();
}

using DisplayGpuCrashBrowserTest = InProcessBrowserTest;

class TestDisplayObserver : public display::DisplayObserver {
 public:
  explicit TestDisplayObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {
    display::Screen::Get()->AddObserver(this);
  }

  TestDisplayObserver(const TestDisplayObserver&) = delete;
  const TestDisplayObserver& operator=(const TestDisplayObserver&) = delete;

  ~TestDisplayObserver() override {
    display::Screen::Get()->RemoveObserver(this);
  }

  // display::DisplayObserver:
  void OnDisplaysRemoved(const display::Displays& removed_displays) override {
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

class TestSurfaceIdObserver : public ui::CompositorObserver {
 public:
  TestSurfaceIdObserver(ui::Compositor* compositor,
                        base::OnceClosure quit_closure)
      : compositor_(compositor), quit_closure_(std::move(quit_closure)) {
    compositor_->AddObserver(this);
  }

  TestSurfaceIdObserver(const TestSurfaceIdObserver&) = delete;
  const TestSurfaceIdObserver& operator=(const TestSurfaceIdObserver&) = delete;

  ~TestSurfaceIdObserver() override { compositor_->RemoveObserver(this); }

  void OnFirstSurfaceActivation(ui::Compositor* compositor,
                                const viz::SurfaceInfo& surface_info) override {
    std::move(quit_closure_).Run();
  }

 private:
  raw_ptr<ui::Compositor> compositor_;
  base::OnceClosure quit_closure_;
};

}  // namespace

// TODO(crbug.com/388451843): Flaky on ChromeOS release.
IN_PROC_BROWSER_TEST_F(DisplayGpuCrashBrowserTest, DISABLED_CrashInMirror) {
  display::test::DisplayManagerTestApi test_api(GetDisplayManager());
  test_api.UpdateDisplay("1300x1000,1000x800");
  ASSERT_EQ(2u, GetDisplayManager()->GetNumDisplays());

  int64_t secondary_id = test_api.GetSecondaryDisplay().id();
  {
    base::RunLoop loop;
    TestDisplayObserver observer(loop.QuitClosure());
    GetDisplayManager()->SetMirrorMode(display::MirrorMode::kNormal,
                                       std::nullopt);
    loop.Run();
  }
  {
    auto* primary_root = ash::Shell::GetPrimaryRootWindow();
    auto local_surface_id = primary_root->GetLocalSurfaceId();

    base::RunLoop loop;
    TestSurfaceIdObserver surface_id_observer(
        primary_root->GetHost()->compositor(), loop.QuitClosure());

    SimulateGpuCrash();

    loop.Run();
    EXPECT_NE(local_surface_id, primary_root->GetLocalSurfaceId());

    ash::MirrorWindowController* mirror = ash::Shell::Get()
                                              ->window_tree_host_manager()
                                              ->mirror_window_controller();
    ASSERT_EQ(mirror->GetAllRootWindows().size(), 1u);
    const aura::Window* mirror_window =
        mirror->GetMirrorWindowForDisplayIdForTest(secondary_id);
    EXPECT_TRUE(mirror_window->layer()->has_external_content());
    EXPECT_EQ(primary_root->GetSurfaceId(),
              mirror_window->layer()->external_content_surface_id());
  }
}

// TODO(crbug.com/368538284): Debug build prints too many error messages while
// waiting for GPU restart, which causes test failure on bots.
IN_PROC_BROWSER_TEST_F(DisplayGpuCrashBrowserTest, CrashInUnified) {
  auto* display_manager = GetDisplayManager();
  display_manager->SetUnifiedDesktopEnabled(true);

  display::test::DisplayManagerTestApi test_api(display_manager);
  test_api.UpdateDisplay("1300x1000,1000x800");

  const auto mirroring_displays =
      display_manager->software_mirroring_display_list();
  ASSERT_EQ(1u, display_manager->GetNumDisplays());
  ASSERT_EQ(2u, mirroring_displays.size());

  auto* primary_root = ash::Shell::GetPrimaryRootWindow();
  auto local_surface_id = primary_root->GetLocalSurfaceId();

  base::RunLoop loop;
  TestSurfaceIdObserver surface_id_observer(
      primary_root->GetHost()->compositor(), loop.QuitClosure());

  SimulateGpuCrash();

  loop.Run();
  EXPECT_NE(local_surface_id, primary_root->GetLocalSurfaceId());

  auto* mirror_window_controller =
      ash::Shell::Get()->window_tree_host_manager()->mirror_window_controller();
  ASSERT_EQ(mirror_window_controller->GetAllRootWindows().size(), 2u);

  for (auto display : mirroring_displays) {
    SCOPED_TRACE("mirroring_display=" + display.ToString());
    const aura::Window* mirror_window =
        mirror_window_controller->GetMirrorWindowForDisplayIdForTest(
            display.id());
    EXPECT_TRUE(mirror_window->layer()->has_external_content());
    EXPECT_EQ(primary_root->GetSurfaceId(),
              mirror_window->layer()->external_content_surface_id());
  }
}
