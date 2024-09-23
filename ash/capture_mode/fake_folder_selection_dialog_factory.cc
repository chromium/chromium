// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/fake_folder_selection_dialog_factory.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {
FakeFolderSelectionDialogFactory* g_factory_instance = nullptr;
}  // namespace

// -----------------------------------------------------------------------------
// FakeFolderSelectionDialog:

class FakeFolderSelectionDialog : public ui::SelectFileDialog {
 public:
  FakeFolderSelectionDialog(Listener* listener,
                            std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

  aura::Window* GetDialogWindow() {
    DCHECK(dialog_widget_);
    return dialog_widget_->GetNativeWindow();
  }

  void AcceptPath(const base::FilePath& path) {
    DCHECK(dialog_widget_);
    if (listener_) {
      listener_->FileSelected(ui::SelectedFileInfo(path), /*index=*/0);
    }
    DismissDialog();
  }

  void CancelDialog() {
    DCHECK(dialog_widget_);
    if (listener_) {
      listener_->FileSelectionCanceled();
    }
    DismissDialog();
  }

  // ui::BaseShellDialog:
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    // The dialog is not shown modally.
    return false;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }

 protected:
  ~FakeFolderSelectionDialog() override {
    if (g_factory_instance)
      g_factory_instance->OnDialogDeleted(this);
  }

  // ui::SelectFileDialog:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    dialog_widget_ = views::UniqueWidgetPtr(std::make_unique<views::Widget>());
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_POPUP);
    widget_params.parent = owning_window;
    widget_params.bounds = owning_window->GetRootWindow()->bounds();
    widget_params.bounds.Inset(gfx::Insets(20));
    widget_params.name = "FakeFolderSelectionDialogWidget";
    dialog_widget_->Init(std::move(widget_params));
    dialog_widget_->Show();
  }

 private:
  void DismissDialog() {
    dialog_widget_->CloseNow();
    // |this| is deleted after the above call.
    DCHECK(!g_factory_instance || !(g_factory_instance->dialog_));
  }

  // ui::SelectFileDialog:
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

  views::UniqueWidgetPtr dialog_widget_;
};

// -----------------------------------------------------------------------------
// FakeFolderSelectionDialogFactory:

// static
void FakeFolderSelectionDialogFactory::Start() {
  ui::SelectFileDialog::SetFactory(
      // Private constructor.
      base::WrapUnique(new FakeFolderSelectionDialogFactory()));
}

// static
void FakeFolderSelectionDialogFactory::Stop() {
  ui::SelectFileDialog::SetFactory(nullptr);
  DCHECK(!g_factory_instance);
}

// static
FakeFolderSelectionDialogFactory* FakeFolderSelectionDialogFactory::Get() {
  DCHECK(g_factory_instance);
  return g_factory_instance;
}

aura::Window* FakeFolderSelectionDialogFactory::GetDialogWindow() {
  DCHECK(dialog_);
  return dialog_->GetDialogWindow();
}

void FakeFolderSelectionDialogFactory::AcceptPath(const base::FilePath& path) {
  DCHECK(dialog_);
  dialog_->AcceptPath(path);
}

void FakeFolderSelectionDialogFactory::CancelDialog() {
  DCHECK(dialog_);
  dialog_->CancelDialog();
}

ui::SelectFileDialog* FakeFolderSelectionDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  dialog_ = new FakeFolderSelectionDialog(listener, std::move(policy));
  return dialog_;
}

FakeFolderSelectionDialogFactory::FakeFolderSelectionDialogFactory() {
  DCHECK(!g_factory_instance);
  g_factory_instance = this;
}

FakeFolderSelectionDialogFactory::~FakeFolderSelectionDialogFactory() {
  DCHECK_EQ(g_factory_instance, this);
  g_factory_instance = nullptr;
}

void FakeFolderSelectionDialogFactory::OnDialogDeleted(
    FakeFolderSelectionDialog* dialog) {
  DCHECK_EQ(dialog_, dialog);
  dialog_ = nullptr;
}

}  // namespace ash
