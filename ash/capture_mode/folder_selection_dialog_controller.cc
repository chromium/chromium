// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/folder_selection_dialog_controller.h"

#include <memory>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/files/file_path.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"

namespace ash {

namespace {

// Returns true if |event| is targeting a window in the subtree rooted at
// |window|.
bool IsEventTargetingWindowInSubtree(const ui::Event* event,
                                     const aura::Window* window) {
  DCHECK(window);

  auto* target = static_cast<aura::Window*>(event->target());
  return window->Contains(target);
}

}  // namespace

FolderSelectionDialogController::FolderSelectionDialogController(
    Delegate* delegate,
    aura::Window* root)
    : delegate_(delegate),
      // The SettingBubbleContainer was chosen since it's below the virtual
      // keyboard container, and therefore users can use the VK to interact with
      // this dialog e.g. to rename a folder.
      dialog_background_dimmer_(
          root->GetChildById(kShellWindowId_SettingBubbleContainer)),
      select_folder_dialog_(ui::SelectFileDialog::Create(
          /*listener=*/this,
          /*policy=*/nullptr)) {
  DCHECK(delegate_);
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto* owner = dialog_background_dimmer_.window();
  owner->SetId(kShellWindowId_CaptureModeFolderSelectionDialogOwner);
  window_observation_.Observe(wm::TransientWindowManager::GetOrCreate(owner));

  dialog_background_dimmer_.SetDimColor(kColorAshShieldAndBase40);
  owner->Show();

  select_folder_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_FOLDER,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_SAVE_TO_DIALOG_TITLE),
      /*default_path=*/base::FilePath(),
      /*file_types=*/nullptr,
      /*file_type_index=*/0,
      /*default_extension=*/base::FilePath::StringType(),
      /*owning_window=*/owner);
}

FolderSelectionDialogController::~FolderSelectionDialogController() {
  if (select_folder_dialog_)
    select_folder_dialog_->ListenerDestroyed();
}

bool FolderSelectionDialogController::ShouldConsumeEvent(
    const ui::Event* event) const {
  if (!dialog_window_)
    return true;

  if (IsEventTargetingWindowInSubtree(event, dialog_window_))
    return false;

  // The event maybe targeting a virtual keyboard window that is being used to
  // interact with the dialog. In this case the event should not be consumed.
  auto* keyboard_ui_controller = keyboard::KeyboardUIController::Get();
  DCHECK(keyboard_ui_controller);

  if (!keyboard_ui_controller->IsKeyboardVisible())
    return true;

  auto* keyboard_window = keyboard_ui_controller->GetKeyboardWindow();
  if (!keyboard_window ||
      keyboard_window->GetRootWindow() != dialog_window_->GetRootWindow()) {
    return true;
  }

  return !IsEventTargetingWindowInSubtree(event, keyboard_window);
}

void FolderSelectionDialogController::FileSelected(
    const ui::SelectedFileInfo& file,
    int index) {
  did_user_select_a_folder_ = true;
  delegate_->OnFolderSelected(file.path());
}

void FolderSelectionDialogController::OnTransientChildAdded(
    aura::Window* window,
    aura::Window* transient) {
  DCHECK_EQ(window, dialog_background_dimmer_.window());
  DCHECK(!dialog_window_);

  dialog_window_ = transient;

  // The dialog should never resize, minimize or maximize.
  auto* widget = views::Widget::GetWidgetForNativeWindow(dialog_window_);
  DCHECK(widget);
  views::WidgetDelegate* widget_delegate = widget->widget_delegate();
  DCHECK(widget_delegate);
  widget_delegate->SetCanResize(false);
  widget_delegate->SetCanMinimize(false);
  widget_delegate->SetCanMaximize(false);
  widget_delegate->SetCanFullscreen(false);

  delegate_->OnSelectionWindowAdded();

  if (on_dialog_window_added_callback_for_test_)
    std::move(on_dialog_window_added_callback_for_test_).Run();
}

void FolderSelectionDialogController::OnTransientChildRemoved(
    aura::Window* window,
    aura::Window* transient) {
  DCHECK_EQ(window, dialog_background_dimmer_.window());
  DCHECK(dialog_window_);
  DCHECK_EQ(transient, dialog_window_);

  dialog_window_ = nullptr;
  delegate_->OnSelectionWindowClosed();
  // |this| will be deleted after the above call.
}

}  // namespace ash
