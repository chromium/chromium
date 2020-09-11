// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history_controller.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_menu_model_adapter.h"
#include "ash/clipboard/clipboard_history_resource_manager.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_data.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"
#include "ui/base/clipboard/clipboard_non_backed.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {

namespace {

ui::ClipboardNonBacked* GetClipboard() {
  auto* clipboard = ui::ClipboardNonBacked::GetForCurrentThread();
  DCHECK(clipboard);
  return clipboard;
}

bool IsRectContainedByAnyDisplay(const gfx::Rect& rect) {
  const std::vector<display::Display>& displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const auto& display : displays) {
    if (display.bounds().Contains(rect))
      return true;
  }
  return false;
}

}  // namespace

// ClipboardHistoryController::AcceleratorTarget -------------------------------

class ClipboardHistoryController::AcceleratorTarget
    : public ui::AcceleratorTarget {
 public:
  explicit AcceleratorTarget(ClipboardHistoryController* controller)
      : controller_(controller),
        show_menu_combo_(
            ui::Accelerator(/*key_code=*/ui::VKEY_V,
                            /*modifiers=*/ui::EF_COMMAND_DOWN,
                            /*key_state=*/ui::Accelerator::KeyState::PRESSED)),
        delete_selected_(ui::Accelerator(
            /*key_code=*/ui::VKEY_BACK,
            /*modifiers=*/ui::EF_NONE,
            /*key_state=*/ui::Accelerator::KeyState::PRESSED)) {}
  AcceleratorTarget(const AcceleratorTarget&) = delete;
  AcceleratorTarget& operator=(const AcceleratorTarget&) = delete;
  ~AcceleratorTarget() override = default;

  void Init() {
    // Register, but no need to unregister because this outlives
    // AcceleratorController.
    Shell::Get()->accelerator_controller()->Register(
        {show_menu_combo_}, /*accelerator_target=*/this);
  }

  void OnMenuShown() {
    Shell::Get()->accelerator_controller()->Register(
        {delete_selected_}, /*accelerator_target=*/this);
  }

  void OnMenuClosed() {
    Shell::Get()->accelerator_controller()->Unregister(
        {delete_selected_}, /*accelerator_target=*/this);
  }

 private:
  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    DCHECK(accelerator == show_menu_combo_ || accelerator == delete_selected_);

    if (accelerator == show_menu_combo_)
      HandleShowMenuCombo();
    else
      HandleDeleteSelected();
    return true;
  }

  bool CanHandleAccelerators() const override {
    return controller_->IsMenuShowing() || controller_->CanShowMenu();
  }

  void HandleShowMenuCombo() {
    if (controller_->IsMenuShowing())
      controller_->ExecuteSelectedMenuItem(show_menu_combo_.modifiers());
    else
      controller_->ShowMenu();
  }

  void HandleDeleteSelected() {
    DCHECK(controller_->IsMenuShowing());
    controller_->DeleteSelectedMenuItemIfAny();
  }

  // The controller responsible for showing the Clipboard History menu.
  ClipboardHistoryController* const controller_;

  // The accelerator to show the menu or execute the selected menu item.
  const ui::Accelerator show_menu_combo_;

  // The accelerator to delete the selected menu item. It is only registered
  // while the menu is showing.
  const ui::Accelerator delete_selected_;
};

// ClipboardHistoryController::MenuDelegate ------------------------------------

class ClipboardHistoryController::MenuDelegate
    : public ui::SimpleMenuModel::Delegate {
 public:
  explicit MenuDelegate(ClipboardHistoryController* controller)
      : controller_(controller) {}
  MenuDelegate(const MenuDelegate&) = delete;
  MenuDelegate& operator=(const MenuDelegate&) = delete;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id == ClipboardHistoryUtil::kDeleteCommandId) {
      controller_->DeleteSelectedMenuItemIfAny();
      return;
    }

    controller_->MenuOptionSelected(command_id, event_flags);
  }

 private:
  // The controller responsible for showing the Clipboard History menu.
  ClipboardHistoryController* const controller_;
};

// ClipboardHistoryController --------------------------------------------------

ClipboardHistoryController::ClipboardHistoryController()
    : clipboard_history_(std::make_unique<ClipboardHistory>()),
      resource_manager_(std::make_unique<ClipboardHistoryResourceManager>(
          clipboard_history_.get())),
      accelerator_target_(std::make_unique<AcceleratorTarget>(this)),
      menu_delegate_(std::make_unique<MenuDelegate>(this)),
      nudge_controller_(std::make_unique<ClipboardNudgeController>(
          clipboard_history_.get())) {}

ClipboardHistoryController::~ClipboardHistoryController() = default;

void ClipboardHistoryController::Init() {
  accelerator_target_->Init();
}

bool ClipboardHistoryController::IsMenuShowing() const {
  return context_menu_ && context_menu_->IsRunning();
}

gfx::Rect ClipboardHistoryController::GetMenuBoundsInScreenForTest() const {
  return context_menu_->GetMenuBoundsInScreenForTest();
}

bool ClipboardHistoryController::CanShowMenu() const {
  return !clipboard_history_->IsEmpty() &&
         clipboard_history_->IsEnabledInCurrentMode();
}

void ClipboardHistoryController::ExecuteSelectedMenuItem(int event_flags) {
  DCHECK(IsMenuShowing());
  auto command = context_menu_->GetSelectedMenuItemCommand();

  // If no menu item is currently selected, we'll fallback to the first item.
  menu_delegate_->ExecuteCommand(
      command.value_or(ClipboardHistoryUtil::kFirstItemCommandId), event_flags);
}

void ClipboardHistoryController::ShowMenu() {
  if (IsMenuShowing() || !CanShowMenu())
    return;

  context_menu_ = ClipboardHistoryMenuModelAdapter::Create(
      menu_delegate_.get(),
      base::BindRepeating(&ClipboardHistoryController::OnMenuClosed,
                          base::Unretained(this)),
      clipboard_history_.get(), resource_manager_.get());
  context_menu_->Run(CalculateAnchorRect());

  DCHECK(IsMenuShowing());
  accelerator_target_->OnMenuShown();
}

void ClipboardHistoryController::MenuOptionSelected(int command_id,
                                                    int event_flags) {
  // Force close the context menu. Failure to do so before dispatching our
  // synthetic key event will result in the context menu consuming the event.
  DCHECK(context_menu_);
  context_menu_->Cancel();

  const ClipboardHistoryItem& selected_item =
      context_menu_->GetItemFromCommandId(command_id);

  auto* clipboard = GetClipboard();
  std::unique_ptr<ui::ClipboardData> original_data;

  // If necessary, replace the clipboard's |original_data| temporarily so that
  // we can paste the selected history item.
  const bool shift_key_pressed = event_flags & ui::EF_SHIFT_DOWN;
  ui::ClipboardDataEndpoint data_dst(ui::EndpointType::kClipboardHistory);
  if (shift_key_pressed ||
      selected_item.data() != *clipboard->GetClipboardData(&data_dst)) {
    std::unique_ptr<ui::ClipboardData> temp_data;
    if (shift_key_pressed) {
      // When the shift key is pressed, we only paste plain text.
      temp_data = std::make_unique<ui::ClipboardData>();
      temp_data->set_text(selected_item.data().text());
      ui::ClipboardDataEndpoint* data_src = selected_item.data().source();
      if (data_src) {
        temp_data->set_source(
            std::make_unique<ui::ClipboardDataEndpoint>(*data_src));
      }
    } else {
      temp_data = std::make_unique<ui::ClipboardData>(selected_item.data());
    }

    // Pause clipboard history when manipulating the clipboard for a paste.
    ClipboardHistory::ScopedPause scoped_pause(clipboard_history_.get());
    original_data = clipboard->WriteClipboardData(std::move(temp_data));
  }

  ui::KeyEvent synthetic_key_event(ui::ET_KEY_PRESSED, ui::VKEY_V,
                                   static_cast<ui::DomCode>(0),
                                   ui::EF_CONTROL_DOWN);
  auto* host = ash::GetWindowTreeHostForDisplay(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  DCHECK(host);
  host->DeliverEventToSink(&synthetic_key_event);

  if (!original_data)
    return;

  // Replace the original item back on top of the clipboard. Some apps take a
  // long time to receive the paste event, also some apps will read from the
  // clipboard multiple times per paste. Wait a bit before replacing the item
  // back onto the clipboard.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<ClipboardHistoryController>& weak_ptr,
             std::unique_ptr<ui::ClipboardData> original_data) {
            // When restoring the original item back on top of the clipboard we
            // need to pause clipboard history. Failure to do so will result in
            // the original item being re-recorded when this restoration step
            // should actually be opaque to the user.
            std::unique_ptr<ClipboardHistory::ScopedPause> scoped_pause;
            if (weak_ptr) {
              scoped_pause = std::make_unique<ClipboardHistory::ScopedPause>(
                  weak_ptr->clipboard_history_.get());
            }
            GetClipboard()->WriteClipboardData(std::move(original_data));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(original_data)),
      base::TimeDelta::FromMilliseconds(200));
}

void ClipboardHistoryController::DeleteSelectedMenuItemIfAny() {
  DCHECK(context_menu_);
  auto selected_command = context_menu_->GetSelectedMenuItemCommand();

  // Return early if no item is selected.
  if (!selected_command.has_value())
    return;

  DCHECK_GE(*selected_command, ClipboardHistoryUtil::kFirstItemCommandId);

  clipboard_history_->RemoveItemForId(
      context_menu_->GetItemFromCommandId(*selected_command).id());

  // If the item to be deleted is the last one, close the whole menu.
  if (context_menu_->GetMenuItemsCount() == 1) {
    context_menu_->Cancel();
    context_menu_.reset();
    return;
  }

  context_menu_->RemoveMenuItemWithCommandId(*selected_command);
}

gfx::Rect ClipboardHistoryController::CalculateAnchorRect() const {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  auto* host = ash::GetWindowTreeHostForDisplay(display.id());

  // Some web apps render the caret in an IFrame, and we will not get the
  // bounds in that case.
  // TODO(https://crbug.com/1099930): Show the menu in the middle of the
  // webview if the bounds are empty.
  ui::TextInputClient* text_input_client =
      host->GetInputMethod()->GetTextInputClient();

  // `text_input_client` may be null. For example, in clamshell mode and without
  // any window open.
  const gfx::Rect textfield_bounds =
      text_input_client ? text_input_client->GetCaretBounds() : gfx::Rect();

  // Note that the width of caret's bounds may be zero in some views (such as
  // the search bar of Google search web page). So we cannot use
  // gfx::Size::IsEmpty() here. In addition, the applications using IFrame may
  // provide unreliable `textfield_bounds` which are not fully contained by the
  // display bounds.
  const bool textfield_bounds_are_valid =
      textfield_bounds.size() != gfx::Size() &&
      IsRectContainedByAnyDisplay(textfield_bounds);

  if (textfield_bounds_are_valid)
    return textfield_bounds;

  return gfx::Rect(display::Screen::GetScreen()->GetCursorScreenPoint(),
                   gfx::Size());
}

void ClipboardHistoryController::OnMenuClosed() {
  accelerator_target_->OnMenuClosed();
}

}  // namespace ash
