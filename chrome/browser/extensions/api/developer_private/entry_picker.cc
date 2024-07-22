// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/entry_picker.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

bool g_skip_picker_for_test = false;

std::optional<ui::SelectedFileInfo>& FileToBePickedForTest() {
  static base::NoDestructor<std::optional<ui::SelectedFileInfo>> file;
  return *file;
}

}  // namespace

namespace extensions {

namespace api {

EntryPicker::EntryPicker(EntryPickerClient* client,
                         content::WebContents* web_contents,
                         ui::SelectFileDialog::Type picker_type,
                         const base::FilePath& last_directory,
                         const std::u16string& select_title,
                         const ui::SelectFileDialog::FileTypeInfo& info,
                         int file_type_index)
    : client_(client) {
  if (g_skip_picker_for_test) {
    if (FileToBePickedForTest().has_value()) {  // IN-TEST
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&EntryPicker::FileSelected, base::Unretained(this),
                         FileToBePickedForTest().value(), 1));  // IN-TEST
    } else {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&EntryPicker::FileSelectionCanceled,
                                    base::Unretained(this)));
    }
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  gfx::NativeWindow owning_window = web_contents ?
      platform_util::GetTopLevel(web_contents->GetNativeView()) :
      nullptr;

  select_file_dialog_->SelectFile(picker_type, select_title, last_directory,
                                  &info, file_type_index,
                                  base::FilePath::StringType(), owning_window);
}

EntryPicker::~EntryPicker() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void EntryPicker::FileSelected(const ui::SelectedFileInfo& file, int index) {
  client_->FileSelected(file.path());
  delete this;
}

void EntryPicker::FileSelectionCanceled() {
  client_->FileSelectionCanceled();
  delete this;
}

// static
void EntryPicker::SkipPickerAndAlwaysSelectPathForTest(
    const base::FilePath& path) {
  g_skip_picker_for_test = true;
  FileToBePickedForTest().emplace(path, path);  // IN-TEST
}

// static
void EntryPicker::SkipPickerAndAlwaysCancelForTest() {
  g_skip_picker_for_test = true;
  FileToBePickedForTest().reset();  // IN-TEST
}

// static
void EntryPicker::StopSkippingPickerForTest() {
  g_skip_picker_for_test = false;
}

}  // namespace api

}  // namespace extensions
