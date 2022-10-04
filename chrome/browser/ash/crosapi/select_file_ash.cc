// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/select_file_ash.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "base/cxx17_backports.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/crosapi/window_util.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "chromeos/crosapi/mojom/select_file.mojom-shared.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"

namespace crosapi {
namespace {

ui::SelectFileDialog::Type GetUiType(mojom::SelectFileDialogType type) {
  switch (type) {
    case mojom::SelectFileDialogType::kFolder:
      return ui::SelectFileDialog::Type::SELECT_FOLDER;
    case mojom::SelectFileDialogType::kUploadFolder:
      return ui::SelectFileDialog::Type::SELECT_UPLOAD_FOLDER;
    case mojom::SelectFileDialogType::kExistingFolder:
      return ui::SelectFileDialog::Type::SELECT_EXISTING_FOLDER;
    case mojom::SelectFileDialogType::kOpenFile:
      return ui::SelectFileDialog::Type::SELECT_OPEN_FILE;
    case mojom::SelectFileDialogType::kOpenMultiFile:
      return ui::SelectFileDialog::Type::SELECT_OPEN_MULTI_FILE;
    case mojom::SelectFileDialogType::kSaveAsFile:
      return ui::SelectFileDialog::Type::SELECT_SAVEAS_FILE;
  }
}

ui::SelectFileDialog::FileTypeInfo::AllowedPaths GetUiAllowedPaths(
    mojom::AllowedPaths allowed_paths) {
  switch (allowed_paths) {
    case mojom::AllowedPaths::kAnyPath:
      return ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
    case mojom::AllowedPaths::kNativePath:
      return ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
    case mojom::AllowedPaths::kAnyPathOrUrl:
      return ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  }
}

// Manages a single open/save dialog. There may be multiple dialogs showing at
// the same time. Deletes itself when the dialog is closed.
class SelectFileDialogHolder : public ui::SelectFileDialog::Listener {
 public:
  // |owner_window| is either the ShellSurface window that spawned the dialog,
  // or an ash container window for a modeless dialog.
  SelectFileDialogHolder(aura::Window* owner_window,
                         mojom::SelectFileOptionsPtr options,
                         mojom::SelectFile::SelectCallback callback)
      : select_callback_(std::move(callback)) {
    DCHECK(owner_window);
    // Policy is null because showing the file-dialog-blocked infobar is handled
    // client-side in lacros-chrome.
    select_file_dialog_ =
        SelectFileDialogExtension::Create(this, /*policy=*/nullptr);

    SelectFileDialogExtension::Owner owner;
    owner.is_lacros = true;
    owner.window = owner_window;
    owner.lacros_window_id = options->owning_shell_window_id;
    if (options->caller.has_value()) {
      owner.dialog_caller.emplace(options->caller.value().spec());
    }

    int file_type_index = 0;
    if (options->file_types) {
      file_types_ = std::make_unique<ui::SelectFileDialog::FileTypeInfo>();
      file_types_->extensions = options->file_types->extensions;
      // Only apply description overrides if the right number are provided.
      if (options->file_types->extensions.size() ==
          options->file_types->extension_description_overrides.size()) {
        file_types_->extension_description_overrides =
            options->file_types->extension_description_overrides;
      }
      // Index is 1-based (hence range 1 to size()), but 0 is allowed because it
      // means "no selection". See ui::SelectFileDialog::SelectFile().
      file_type_index =
          base::clamp(options->file_types->default_file_type_index, 0,
                      static_cast<int>(file_types_->extensions.size()));
      file_types_->include_all_files = options->file_types->include_all_files;
      file_types_->allowed_paths =
          GetUiAllowedPaths(options->file_types->allowed_paths);
    }

    // |default_extension| is unused on Chrome OS.
    select_file_dialog_->SelectFileWithFileManagerParams(
        GetUiType(options->type), options->title, options->default_path,
        file_types_.get(), file_type_index,
        /*params=*/nullptr, owner,
        /*search_query=*/"",
        /*show_android_picker_apps=*/false);
  }

  SelectFileDialogHolder(const SelectFileDialogHolder&) = delete;
  SelectFileDialogHolder& operator=(const SelectFileDialogHolder&) = delete;
  ~SelectFileDialogHolder() override = default;

 private:
  // ui::SelectFileDialog::Listener:
  void FileSelected(const base::FilePath& path,
                    int file_type_index,
                    void* params) override {
    FileSelectedWithExtraInfo(ui::SelectedFileInfo(path, path), file_type_index,
                              params);
  }

  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file,
                                 int file_type_index,
                                 void* params) override {
    OnSelected({file}, file_type_index);
  }

  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override {
    MultiFilesSelectedWithExtraInfo(
        ui::FilePathListToSelectedFileInfoList(files), params);
  }

  void MultiFilesSelectedWithExtraInfo(
      const std::vector<ui::SelectedFileInfo>& files,
      void* params) override {
    OnSelected(files, /*file_type_index=*/0);
  }

  void FileSelectionCanceled(void* params) override {
    // Cancel is the same as selecting nothing.
    OnSelected({}, /*file_type_index=*/0);
  }

  // Invokes |select_callback_| with the list of files and deletes this object.
  void OnSelected(const std::vector<ui::SelectedFileInfo>& files,
                  int file_type_index) {
    std::vector<mojom::SelectedFileInfoPtr> mojo_files;
    for (const ui::SelectedFileInfo& file : files) {
      mojom::SelectedFileInfoPtr mojo_file = mojom::SelectedFileInfo::New();
      mojo_file->file_path = file.file_path;
      mojo_file->local_path = file.local_path;
      mojo_file->display_name = file.display_name;
      mojo_file->url = file.url;
      mojo_files.push_back(std::move(mojo_file));
    }
    std::move(select_callback_)
        .Run(mojom::SelectFileResult::kSuccess, std::move(mojo_files),
             file_type_index);
    delete this;
  }

  // Callback run after files are selected or the dialog is canceled.
  mojom::SelectFile::SelectCallback select_callback_;

  // The file select dialog.
  scoped_refptr<SelectFileDialogExtension> select_file_dialog_;

  // Optional file type extension filters.
  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> file_types_;
};

}  // namespace

SelectFileAsh::SelectFileAsh() = default;

SelectFileAsh::~SelectFileAsh() = default;

void SelectFileAsh::BindReceiver(
    mojo::PendingReceiver<mojom::SelectFile> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void SelectFileAsh::Select(mojom::SelectFileOptionsPtr options,
                           SelectCallback callback) {
  aura::Window* owner_window = nullptr;
  if (!options->owning_shell_window_id.empty()) {
    // In the typical case, parent the dialog to the Lacros browser window's
    // shell surface.
    owner_window = GetShellSurfaceWindow(options->owning_shell_window_id);
    // Bail out if the shell surface doesn't exist any more.
    if (!owner_window) {
      std::move(callback).Run(mojom::SelectFileResult::kInvalidShellWindow, {},
                              0);
      return;
    }
  } else {
    // For modeless dialogs, parent the window to the active desk container on
    // the default display.
    owner_window =
        ash::Shell::GetContainer(ash::Shell::GetRootWindowForNewWindows(),
                                 ash::desks_util::GetActiveDeskContainerId());
  }
  // Deletes itself when the dialog closes.
  new SelectFileDialogHolder(owner_window, std::move(options),
                             std::move(callback));
}

}  // namespace crosapi
