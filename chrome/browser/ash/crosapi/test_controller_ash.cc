// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_controller_ash.h"

#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_source.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"

namespace crosapi {

namespace {

// Returns whether the dispatcher or target was destroyed.
bool Dispatch(aura::WindowTreeHost* host, ui::Event* event) {
  ui::EventDispatchDetails dispatch_details =
      host->GetEventSource()->SendEventToSink(event);
  return dispatch_details.dispatcher_destroyed ||
         dispatch_details.target_destroyed;
}

// Returns whether the dispatcher or target was destroyed.
bool DispatchMouseEvent(aura::Window* window, ui::EventType type) {
  const gfx::Point center = window->bounds().CenterPoint();
  ui::MouseEvent press(type, center, center, ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  return Dispatch(window->GetHost(), &press);
}

// Enables or disables tablet mode and waits for the transition to finish.
void SetTabletModeEnabled(bool enabled) {
  // This does not use ShellTestApi or TabletModeControllerTestApi because those
  // are implemented in test-only files.
  ash::TabletMode::Waiter waiter(enabled);
  ash::Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enabled);
  waiter.Wait();
}

}  // namespace

TestControllerAsh::TestControllerAsh() = default;
TestControllerAsh::~TestControllerAsh() = default;

void TestControllerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::TestController> receiver) {
// This interface is not available on production devices. It's only needed for
// tests that run on Linux-chrome so no reason to expose it.
#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  LOG(ERROR) << "Ash does not support TestController on devices";
#else
  receivers_.Add(this, std::move(receiver));
#endif
}

void TestControllerAsh::DoesWindowExist(const std::string& window_id,
                                        DoesWindowExistCallback callback) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  std::move(callback).Run(window != nullptr);
}

void TestControllerAsh::ClickWindow(const std::string& window_id) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window)
    return;
  bool destroyed = DispatchMouseEvent(window, ui::ET_MOUSE_PRESSED);
  if (!destroyed) {
    DispatchMouseEvent(window, ui::ET_MOUSE_RELEASED);
  }
}

void TestControllerAsh::EnterOverviewMode(EnterOverviewModeCallback callback) {
  overview_waiters_.push_back(std::make_unique<OverviewWaiter>(
      /*wait_for_enter=*/true, std::move(callback), this));
  ash::Shell::Get()->overview_controller()->StartOverview();
}

void TestControllerAsh::ExitOverviewMode(ExitOverviewModeCallback callback) {
  overview_waiters_.push_back(std::make_unique<OverviewWaiter>(
      /*wait_for_enter=*/false, std::move(callback), this));
  ash::Shell::Get()->overview_controller()->EndOverview();
}

void TestControllerAsh::EnterTabletMode(EnterTabletModeCallback callback) {
  SetTabletModeEnabled(true);
  std::move(callback).Run();
}

void TestControllerAsh::ExitTabletMode(ExitTabletModeCallback callback) {
  SetTabletModeEnabled(false);
  std::move(callback).Run();
}

void TestControllerAsh::SendTouchEvent(const std::string& window_id,
                                       mojom::TouchEventType type,
                                       uint8_t pointer_id,
                                       const gfx::PointF& location_in_window,
                                       SendTouchEventCallback cb) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(cb).Run();
    return;
  }
  // Newer lacros might send an enum we don't know about.
  if (!mojom::IsKnownEnumValue(type)) {
    LOG(WARNING) << "Unknown event type: " << type;
    std::move(cb).Run();
    return;
  }
  ui::EventType event_type;
  switch (type) {
    case mojom::TouchEventType::kUnknown:
      // |type| is not optional, so kUnknown is never expected.
      NOTREACHED();
      return;
    case mojom::TouchEventType::kPressed:
      event_type = ui::ET_TOUCH_PRESSED;
      break;
    case mojom::TouchEventType::kMoved:
      event_type = ui::ET_TOUCH_MOVED;
      break;
    case mojom::TouchEventType::kReleased:
      event_type = ui::ET_TOUCH_RELEASED;
      break;
    case mojom::TouchEventType::kCancelled:
      event_type = ui::ET_TOUCH_CANCELLED;
      break;
  }
  // Compute location relative to display root window.
  gfx::PointF location_in_root(location_in_window);
  aura::Window::ConvertPointToTarget(window, window->GetRootWindow(),
                                     &location_in_root);
  ui::PointerDetails details(ui::EventPointerType::kTouch, pointer_id, 1.0f,
                             1.0f, 0.0f);
  ui::TouchEvent touch_event(event_type, location_in_window, location_in_root,
                             ui::EventTimeForNow(), details);
  Dispatch(window->GetHost(), &touch_event);
  std::move(cb).Run();
}

void TestControllerAsh::GetWindowPositionInScreen(
    const std::string& window_id,
    GetWindowPositionInScreenCallback cb) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(cb).Run(base::nullopt);
    return;
  }
  std::move(cb).Run(window->GetBoundsInScreen().origin());
}

void TestControllerAsh::WaiterFinished(OverviewWaiter* waiter) {
  for (size_t i = 0; i < overview_waiters_.size(); ++i) {
    if (waiter == overview_waiters_[i].get()) {
      std::unique_ptr<OverviewWaiter> waiter = std::move(overview_waiters_[i]);
      overview_waiters_.erase(overview_waiters_.begin() + i);

      // Delete asynchronously to avoid re-entrancy. This is safe because the
      // class will never use |test_controller_| after this callback.
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                      std::move(waiter));
      break;
    }
  }
}

// This class waits for overview mode to either enter or exit and fires a
// callback. This class will fire the callback at most once.
class TestControllerAsh::OverviewWaiter : public ash::OverviewObserver {
 public:
  OverviewWaiter(bool wait_for_enter,
                 base::OnceClosure closure,
                 TestControllerAsh* test_controller)
      : wait_for_enter_(wait_for_enter),
        closure_(std::move(closure)),
        test_controller_(test_controller) {
    ash::Shell::Get()->overview_controller()->AddObserver(this);
  }
  OverviewWaiter(const OverviewWaiter&) = delete;
  OverviewWaiter& operator=(const OverviewWaiter&) = delete;
  ~OverviewWaiter() override {
    ash::Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool canceled) override {
    if (wait_for_enter_) {
      if (closure_) {
        std::move(closure_).Run();
        DCHECK(test_controller_);
        TestControllerAsh* controller = test_controller_;
        test_controller_ = nullptr;
        controller->WaiterFinished(this);
      }
    }
  }

  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    if (!wait_for_enter_) {
      if (closure_) {
        std::move(closure_).Run();
        DCHECK(test_controller_);
        TestControllerAsh* controller = test_controller_;
        test_controller_ = nullptr;
        controller->WaiterFinished(this);
      }
    }
  }

 private:
  // If true, waits for enter. Otherwise waits for exit.
  const bool wait_for_enter_;
  base::OnceClosure closure_;

  // The test controller owns this object so is never invalid.
  TestControllerAsh* test_controller_;
};

}  // namespace crosapi
