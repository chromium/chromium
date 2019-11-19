// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_ash.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace ash {

namespace {

// Intentionally leaked in production.
display::Screen* screen_for_shutdown = nullptr;

display::DisplayManager* GetDisplayManager() {
  return Shell::Get()->display_manager();
}

class ScreenForShutdown : public display::Screen {
 public:
  explicit ScreenForShutdown(display::Screen* screen_ash)
      : display_list_(screen_ash->GetAllDisplays()),
        primary_display_(screen_ash->GetPrimaryDisplay()) {
    SetDisplayForNewWindows(primary_display_.id());
  }

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  bool IsWindowUnderCursor(gfx::NativeWindow window) override { return false; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return NULL;
  }
  int GetNumDisplays() const override { return display_list_.size(); }
  const std::vector<display::Display>& GetAllDisplays() const override {
    return display_list_;
  }
  display::Display GetDisplayNearestWindow(
      gfx::NativeView view) const override {
    return primary_display_;
  }
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    return *display::FindDisplayNearestPoint(display_list_, point);
  }
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    const display::Display* matching =
        display::FindDisplayWithBiggestIntersection(display_list_, match_rect);
    // Fallback to the primary display if there is no matching display.
    return matching ? *matching : GetPrimaryDisplay();
  }
  display::Display GetPrimaryDisplay() const override {
    return primary_display_;
  }
  void AddObserver(display::DisplayObserver* observer) override {
    NOTREACHED() << "Observer should not be added during shutdown";
  }
  void RemoveObserver(display::DisplayObserver* observer) override {}

 private:
  const std::vector<display::Display> display_list_;
  const display::Display primary_display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenForShutdown);
};

}  // namespace

ScreenAsh::ScreenAsh() = default;

ScreenAsh::~ScreenAsh() = default;

gfx::Point ScreenAsh::GetCursorScreenPoint() {
  return aura::Env::GetInstance()->last_mouse_location();
}

bool ScreenAsh::IsWindowUnderCursor(gfx::NativeWindow window) {
  return window->Contains(GetWindowAtScreenPoint(
      display::Screen::GetScreen()->GetCursorScreenPoint()));
}

gfx::NativeWindow ScreenAsh::GetWindowAtScreenPoint(const gfx::Point& point) {
  aura::Window* root_window = window_util::GetRootWindowAt(point);
  aura::client::ScreenPositionClient* position_client =
      aura::client::GetScreenPositionClient(root_window);

  gfx::Point local_point = point;
  if (position_client)
    position_client->ConvertPointFromScreen(root_window, &local_point);

  return root_window->GetEventHandlerForPoint(local_point);
}

int ScreenAsh::GetNumDisplays() const {
  return GetDisplayManager()->GetNumDisplays();
}

const std::vector<display::Display>& ScreenAsh::GetAllDisplays() const {
  return GetDisplayManager()->active_display_list();
}

display::Display ScreenAsh::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  if (!window)
    return GetPrimaryDisplay();

  const aura::Window* root_window = window->GetRootWindow();
  if (!root_window)
    return GetPrimaryDisplay();
  const RootWindowSettings* rws = GetRootWindowSettings(root_window);
  int64_t id = rws->display_id;
  // if id is |kInvaildDisplayID|, it's being deleted.
  DCHECK(id != display::kInvalidDisplayId);
  if (id == display::kInvalidDisplayId)
    return GetPrimaryDisplay();

  display::DisplayManager* display_manager = GetDisplayManager();
  // RootWindow needs Display to determine its device scale factor
  // for non desktop display.
  display::Display mirroring_display =
      display_manager->GetMirroringDisplayById(id);
  if (mirroring_display.is_valid())
    return mirroring_display;
  return display_manager->GetDisplayForId(id);
}

display::Display ScreenAsh::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  const display::Display& display =
      GetDisplayManager()->FindDisplayContainingPoint(point);
  if (display.is_valid())
    return display;
  // Fallback to the display that has the shortest Manhattan distance from
  // the |point|. This is correct in the only areas that matter, namely in the
  // corners between the physical screens.
  return *display::FindDisplayNearestPoint(
      GetDisplayManager()->active_only_display_list(), point);
}

display::Display ScreenAsh::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  if (match_rect.IsEmpty())
    return GetDisplayNearestPoint(match_rect.origin());
  const display::Display* matching =
      display::FindDisplayWithBiggestIntersection(
          GetDisplayManager()->active_only_display_list(), match_rect);
  // Fallback to the primary display if there is no matching display.
  return matching ? *matching : GetPrimaryDisplay();
}

display::Display ScreenAsh::GetPrimaryDisplay() const {
  if (!WindowTreeHostManager::HasValidPrimaryDisplayId()) {
    // This should only be allowed temporarily when there are no displays
    // available and hence no primary display. In this case we return a default
    // display to avoid crashes for display observers trying to get the primary
    // display when notified with the removal of the last display.
    // https://crbug.com/866714.
    DCHECK(
        Shell::Get()->window_tree_host_manager()->GetAllRootWindows().empty());
    return display::Display::GetDefaultDisplay();
  }

  return GetDisplayManager()->GetDisplayForId(
      WindowTreeHostManager::GetPrimaryDisplayId());
}

void ScreenAsh::AddObserver(display::DisplayObserver* observer) {
  GetDisplayManager()->AddObserver(observer);
}

void ScreenAsh::RemoveObserver(display::DisplayObserver* observer) {
  GetDisplayManager()->RemoveObserver(observer);
}

// static
display::DisplayManager* ScreenAsh::CreateDisplayManager() {
  std::unique_ptr<ScreenAsh> screen(new ScreenAsh);

  display::Screen* current = display::Screen::GetScreen();
  // If there is no native, or the native was for shutdown,
  // use ash's screen.
  if (!current || current == screen_for_shutdown)
    display::Screen::SetScreenInstance(screen.get());
  display::DisplayManager* manager =
      new display::DisplayManager(std::move(screen));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTabletMode)) {
    manager->set_internal_display_has_accelerometer(true);
  }
  return manager;
}

// static
void ScreenAsh::CreateScreenForShutdown() {
  delete screen_for_shutdown;
  screen_for_shutdown = new ScreenForShutdown(display::Screen::GetScreen());
  display::Screen::SetScreenInstance(screen_for_shutdown);
}

// static
void ScreenAsh::DeleteScreenForShutdown() {
  if (display::Screen::GetScreen() == screen_for_shutdown)
    display::Screen::SetScreenInstance(nullptr);
  delete screen_for_shutdown;
  screen_for_shutdown = nullptr;
}

}  // namespace ash
