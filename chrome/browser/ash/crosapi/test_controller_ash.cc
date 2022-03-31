// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/test_controller_ash.h"

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/tabs/tab_scrubber_chromeos.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_source.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"

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
bool DispatchMouseEvent(aura::Window* window,
                        ui::EventType type,
                        gfx::Point location) {
  ui::MouseEvent press(type, location, location, ui::EventTimeForNow(),
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

void TestControllerAsh::ClickElement(const std::string& element_name,
                                     ClickElementCallback callback) {
  ui::ElementIdentifier id =
      ui::ElementIdentifier::FromName(element_name.c_str());
  if (!id) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto views = views::ElementTrackerViews::GetInstance()
                   ->GetAllMatchingViewsInAnyContext(id);
  if (views.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Pick the first view that matches the element name.
  views::View* view = views[0];

  // We directly send mouse events to the view. It's also possible to use
  // EventGenerator to move the mouse and send a click. Unfortunately, that
  // approach has occasional flakiness. This is presumably due to another window
  // appearing on top of the dialog and taking the mouse events but has not been
  // explicitly diagnosed.
  views::TrackedElementViews* tracked_element =
      views::ElementTrackerViews::GetInstance()->GetElementForView(
          view, /*assign_temporary_id=*/false);
  views::test::InteractionTestUtilSimulatorViews simulator;
  simulator.PressButton(tracked_element,
                        ui::test::InteractionTestUtil::InputType::kMouse);

  std::move(callback).Run(/*success=*/true);
}

void TestControllerAsh::ClickWindow(const std::string& window_id) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window)
    return;
  const gfx::Point center = window->bounds().CenterPoint();
  bool destroyed = DispatchMouseEvent(window, ui::ET_MOUSE_PRESSED, center);
  if (!destroyed) {
    DispatchMouseEvent(window, ui::ET_MOUSE_RELEASED, center);
  }
}

void TestControllerAsh::DoesItemExistInShelf(
    const std::string& item_id,
    DoesItemExistInShelfCallback callback) {
  bool exists = ash::ShelfModel::Get()->ItemIndexByAppID(item_id) != -1;
  std::move(callback).Run(exists);
}

void TestControllerAsh::DoesElementExist(const std::string& element_name,
                                         DoesElementExistCallback callback) {
  ui::ElementIdentifier id =
      ui::ElementIdentifier::FromName(element_name.c_str());
  if (!id) {
    std::move(callback).Run(/*exists=*/false);
    return;
  }

  bool any_elements_exist = !views::ElementTrackerViews::GetInstance()
                                 ->GetAllMatchingViewsInAnyContext(id)
                                 .empty();
  std::move(callback).Run(/*exists=*/any_elements_exist);
}

void TestControllerAsh::DoesWindowExist(const std::string& window_id,
                                        DoesWindowExistCallback callback) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  // A window exists if it is either visible or minimized.
  bool exists = false;
  if (window) {
    auto* window_state = ash::WindowState::Get(window);
    exists = window->IsVisible() || window_state->IsMinimized();
  }
  std::move(callback).Run(exists);
}

void TestControllerAsh::EnterOverviewMode(EnterOverviewModeCallback callback) {
  overview_waiters_.push_back(std::make_unique<OverviewWaiter>(
      /*wait_for_enter=*/true, std::move(callback), this));
  ash::Shell::Get()->overview_controller()->StartOverview(
      ash::OverviewStartAction::kTests);
}

void TestControllerAsh::ExitOverviewMode(ExitOverviewModeCallback callback) {
  overview_waiters_.push_back(std::make_unique<OverviewWaiter>(
      /*wait_for_enter=*/false, std::move(callback), this));
  ash::Shell::Get()->overview_controller()->EndOverview(
      ash::OverviewEndAction::kTests);
}

void TestControllerAsh::EnterTabletMode(EnterTabletModeCallback callback) {
  SetTabletModeEnabled(true);
  std::move(callback).Run();
}

void TestControllerAsh::ExitTabletMode(ExitTabletModeCallback callback) {
  SetTabletModeEnabled(false);
  std::move(callback).Run();
}

void TestControllerAsh::GetContextMenuForShelfItem(
    const std::string& item_id,
    GetContextMenuForShelfItemCallback callback) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(item_id));
  if (!delegate) {
    std::move(callback).Run({});
    return;
  }
  delegate->GetContextMenu(
      /*display_id=*/0,
      base::BindOnce(&TestControllerAsh::OnGetContextMenuForShelfItem,
                     std::move(callback)));
}

void TestControllerAsh::GetMinimizeOnBackKeyWindowProperty(
    const std::string& window_id,
    GetMinimizeOnBackKeyWindowPropertyCallback cb) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(cb).Run(mojom::OptionalBoolean::kUnknown);
    return;
  }
  bool* value = window->GetProperty(ash::kMinimizeOnBackKey);
  if (!value) {
    std::move(cb).Run(mojom::OptionalBoolean::kUnknown);
    return;
  }
  std::move(cb).Run(*value ? mojom::OptionalBoolean::kTrue
                           : mojom::OptionalBoolean::kFalse);
}

void TestControllerAsh::GetWindowPositionInScreen(
    const std::string& window_id,
    GetWindowPositionInScreenCallback cb) {
  aura::Window* window = GetShellSurfaceWindow(window_id);
  if (!window) {
    std::move(cb).Run(absl::nullopt);
    return;
  }
  std::move(cb).Run(window->GetBoundsInScreen().origin());
}

void TestControllerAsh::PinOrUnpinItemInShelf(
    const std::string& item_id,
    bool pin,
    PinOrUnpinItemInShelfCallback callback) {
  int item_index = ash::ShelfModel::Get()->ItemIndexByAppID(item_id);
  if (item_index == -1) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (pin) {
    ash::ShelfModel::Get()->PinExistingItemWithID(item_id);
  } else {
    ash::ShelfModel::Get()->UnpinAppWithID(item_id);
  }
  std::move(callback).Run(/*success=*/true);
}

void TestControllerAsh::SelectItemInShelf(const std::string& item_id,
                                          SelectItemInShelfCallback callback) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(item_id));
  if (!delegate) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto mouse_event = std::make_unique<ui::MouseEvent>(
      ui::ET_MOUSE_PRESSED, gfx::PointF(), gfx::PointF(), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  delegate->ItemSelected(std::move(mouse_event), display::kInvalidDisplayId,
                         ash::LAUNCH_FROM_SHELF,
                         /*callback=*/base::DoNothing(),
                         /*filter_predicate=*/base::NullCallback());
  std::move(callback).Run(/*success=*/true);
}

void TestControllerAsh::SelectContextMenuForShelfItem(
    const std::string& item_id,
    uint32_t index,
    SelectContextMenuForShelfItemCallback callback) {
  ash::ShelfItemDelegate* delegate =
      ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(item_id));
  if (!delegate) {
    std::move(callback).Run(false);
    return;
  }
  delegate->GetContextMenu(
      /*display_id=*/0,
      base::BindOnce(&TestControllerAsh::OnSelectContextMenuForShelfItem,
                     std::move(callback), item_id, index));
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

void TestControllerAsh::RegisterStandaloneBrowserTestController(
    mojo::PendingRemote<mojom::StandaloneBrowserTestController> controller) {
  // At the moment only a single controller is supported.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (standalone_browser_test_controller_.is_bound()) {
    return;
  }
  standalone_browser_test_controller_.Bind(std::move(controller));
  standalone_browser_test_controller_.set_disconnect_handler(base::BindOnce(
      &TestControllerAsh::OnControllerDisconnected, base::Unretained(this)));
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

void TestControllerAsh::OnControllerDisconnected() {
  standalone_browser_test_controller_.reset();
}

void TestControllerAsh::OnGetContextMenuForShelfItem(
    GetContextMenuForShelfItemCallback callback,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  std::vector<std::string> items;
  items.reserve(model->GetItemCount());
  for (int i = 0; i < model->GetItemCount(); ++i) {
    items.push_back(base::UTF16ToUTF8(model->GetLabelAt(i)));
  }
  std::move(callback).Run(std::move(items));
}

void TestControllerAsh::OnSelectContextMenuForShelfItem(
    SelectContextMenuForShelfItemCallback callback,
    const std::string& item_id,
    uint32_t index,
    std::unique_ptr<ui::SimpleMenuModel> model) {
  if (index < model->GetItemCount()) {
    int command_id = model->GetCommandIdAt(index);
    ash::ShelfItemDelegate* delegate =
        ash::ShelfModel::Get()->GetShelfItemDelegate(ash::ShelfID(item_id));
    if (delegate) {
      delegate->ExecuteCommand(/*from_context_menu=*/true, command_id,
                               /*event_flags=*/0, /*display_id=*/0);
      std::move(callback).Run(/*success=*/true);
      return;
    }
  }
  std::move(callback).Run(/*success=*/false);
}

void TestControllerAsh::GetOpenAshBrowserWindows(
    GetOpenAshBrowserWindowsCallback callback) {
  std::move(callback).Run(BrowserList::GetInstance()->size());
}

void TestControllerAsh::CloseAllBrowserWindows(
    CloseAllBrowserWindowsCallback callback) {
  for (auto* browser : *BrowserList::GetInstance()) {
    browser->window()->Close();
  }

  std::move(callback).Run(/*success*/ true);
}

void TestControllerAsh::TriggerTabScrubbing(
    float x_offset,
    TriggerTabScrubbingCallback callback) {
  crosapi::BrowserManager::Get()->HandleTabScrubbing(x_offset);

  // Return whether tab scrubbing logic has started or not in Ash.
  //
  // In practice, it is expected that it does not trigger the scrubbing logic,
  // returning |false|, and signal Lacros to do so.
  bool scrubbing = TabScrubberChromeOS::GetInstance()->IsActivationPending();
  std::move(callback).Run(scrubbing);
}

void TestControllerAsh::SetSelectedSharesheetApp(
    const std::string& app_id,
    SetSelectedSharesheetAppCallback callback) {
  sharesheet::SharesheetService::SetSelectedAppForTesting(
      base::UTF8ToUTF16(app_id));

  std::move(callback).Run();
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
