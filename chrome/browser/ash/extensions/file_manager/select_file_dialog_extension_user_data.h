// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_

#include <string>

#include "base/supports_user_data.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

// Used for attaching SelectFileDialogExtension's user data to its WebContents.
class SelectFileDialogExtensionUserData : public base::SupportsUserData::Data {
 public:
  SelectFileDialogExtensionUserData(const SelectFileDialogExtensionUserData&) =
      delete;
  SelectFileDialogExtensionUserData& operator=(
      const SelectFileDialogExtensionUserData&) = delete;
  ~SelectFileDialogExtensionUserData() override;

  // Attaches the SelectFileDialogExtension's user data to its `web_contents`,
  // that consists of the unique `routing_id`, dialog type, and optionally the
  // `dialog_caller`.
  static void SetDialogDataForWebContents(
      content::WebContents* web_contents,
      const std::string& routing_id,
      ui::SelectFileDialog::Type type,
      absl::optional<policy::DlpFilesController::DlpFileDestination>
          dialog_caller);
  // Returns the SelectFileDialogExtension's routing id attached to
  // `web_contents`, if it can be found.
  static std::string GetRoutingIdForWebContents(
      content::WebContents* web_contents);
  // Returns the SelectFileDialogExtension's dialog type attached to
  // `web_contents`, if it can be found.
  static ui::SelectFileDialog::Type GetDialogTypeForWebContents(
      content::WebContents* web_contents);
  // Returns the SelectFileDialogExtension's caller attached to `web_contents`,
  // if it can be found.
  static absl::optional<policy::DlpFilesController::DlpFileDestination>
  GetDialogCallerForWebContents(content::WebContents* web_contents);

 private:
  SelectFileDialogExtensionUserData(
      const std::string& routing_id,
      ui::SelectFileDialog::Type type,
      absl::optional<policy::DlpFilesController::DlpFileDestination>
          dialog_caller);

  const std::string& routing_id() const { return routing_id_; }

  ui::SelectFileDialog::Type type() const { return type_; }

  absl::optional<policy::DlpFilesController::DlpFileDestination> dialog_caller()
      const {
    return dialog_caller_;
  }

  std::string routing_id_;
  ui::SelectFileDialog::Type type_;
  absl::optional<policy::DlpFilesController::DlpFileDestination> dialog_caller_;
};

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_
