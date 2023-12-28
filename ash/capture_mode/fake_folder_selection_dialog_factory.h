// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_FAKE_FOLDER_SELECTION_DIALOG_FACTORY_H_
#define ASH_CAPTURE_MODE_FAKE_FOLDER_SELECTION_DIALOG_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class FakeFolderSelectionDialog;

// Defines a factory that creates a fake folder selection dialog, and exposes
// APIs to simulate operations on this dialog such as canceling / dismissing and
// accepting a path. This factory allows only the creation of one dialog at a
// time.
class FakeFolderSelectionDialogFactory : public ui::SelectFileDialogFactory {
 public:
  FakeFolderSelectionDialogFactory(const FakeFolderSelectionDialogFactory&) =
      delete;
  FakeFolderSelectionDialogFactory& operator=(
      const FakeFolderSelectionDialogFactory&) = delete;

  ~FakeFolderSelectionDialogFactory() override;

  // Starts using this factory. This creates an instance of this factory and
  // sets it to ui::SelectFileDialog::SetFactory() which takes ownership of the
  // instance. From that point, calls to ui::SelectFileDialog::Create() will
  // create the dialog created by this factory. Get() can be called after this.
  static void Start();

  // Stops using this factory. This should only be called if Start() was ever
  // called, which would reset the factory to its default, and delete any
  // previously created instance of this factory by Start(). Get() should never
  // be called after this.
  static void Stop();

  // Returns the instance of this factory. Can only be called between calls to
  // Start() and Stop();
  static FakeFolderSelectionDialogFactory* Get();

  // Returns the window created for the fake dialog. A dialog must have already
  // been created.
  aura::Window* GetDialogWindow();

  // Accepts the dialog using the given |path| and dismisses the dialog.
  void AcceptPath(const base::FilePath& path);

  // Cancels the dialog without any path selection.
  void CancelDialog();

  // ui::SelectFileDialogFactory:
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override;

 private:
  friend class FakeFolderSelectionDialog;

  FakeFolderSelectionDialogFactory();

  // Called by the dialog when it gets deleted so we can clear |dialog_|.
  void OnDialogDeleted(FakeFolderSelectionDialog* dialog);

  // A reference to the dialog created by this factory.
  raw_ptr<FakeFolderSelectionDialog> dialog_ = nullptr;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_FAKE_FOLDER_SELECTION_DIALOG_FACTORY_H_
