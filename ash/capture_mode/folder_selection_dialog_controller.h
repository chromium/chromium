// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_FOLDER_SELECTION_DIALOG_CONTROLLER_H_
#define ASH_CAPTURE_MODE_FOLDER_SELECTION_DIALOG_CONTROLLER_H_

#include "ash/wm/window_dimmer.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/transient_window_observer.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

// Creates, shows and controls capture mode's folder selection dialog.
class FolderSelectionDialogController : public ui::SelectFileDialog::Listener,
                                        public wm::TransientWindowObserver {
 public:
  class Delegate {
   public:
    // Called to inform the delegate that the user selected the given |path|.
    virtual void OnFolderSelected(const base::FilePath& path) = 0;

    // Called to inform the delegate that the dialog window has been added.
    virtual void OnSelectionWindowAdded() = 0;

    // Called to inform the delegate that the dialog window has been closed.
    // This will be called for both when the user selects and accepts a folder
    // or cancels or closes the dialog without making a selection.
    virtual void OnSelectionWindowClosed() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Constructs and shows a folder selection dialog under the given |root|
  // window.
  FolderSelectionDialogController(Delegate* delegate, aura::Window* root);
  FolderSelectionDialogController(const FolderSelectionDialogController&) =
      delete;
  FolderSelectionDialogController& operator=(
      const FolderSelectionDialogController&) = delete;
  ~FolderSelectionDialogController() override;

  aura::Window* dialog_window() { return dialog_window_; }
  bool did_user_select_a_folder() const { return did_user_select_a_folder_; }

  // Returns false while the dialog window is shown, and the |event| is
  // targeting a window in the dialog subtree (in this case, the
  // |CaptureModeSession| should not consume this event and let it go through to
  // the dialog). Returns true otherwise so the session can consume this event.
  bool ShouldConsumeEvent(const ui::Event* event) const;

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;

  // wm::TransientWindowObserver:
  void OnTransientChildAdded(aura::Window* window,
                             aura::Window* transient) override;
  void OnTransientChildRemoved(aura::Window* window,
                               aura::Window* transient) override;

 private:
  friend class CaptureModeTestApi;

  const raw_ptr<Delegate> delegate_;

  // Dims everything behind the dialog (including the capture bar, the settings
  // menu, and any capture-related UIs). The dimming window is the transient
  // parent of the dialog window.
  WindowDimmer dialog_background_dimmer_;

  // Provides us with the APIs needed to construct a folder selection dialog.
  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;

  // This is the window of the dialog that gets created by
  // |select_folder_dialog_| as a transient child of the dimming window.
  raw_ptr<aura::Window> dialog_window_ = nullptr;

  // It will be set to true when user selects a folder from the dialog.
  bool did_user_select_a_folder_ = false;

  // An optional callback that will be invoked when |dialog_window_| gets added.
  base::OnceClosure on_dialog_window_added_callback_for_test_;

  // We observe the transient window manager of the dimming window to know when
  // the dialog window is added or removed.
  base::ScopedObservation<wm::TransientWindowManager,
                          wm::TransientWindowObserver>
      window_observation_{this};
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_FOLDER_SELECTION_DIALOG_CONTROLLER_H_
