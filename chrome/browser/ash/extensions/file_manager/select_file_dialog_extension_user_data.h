// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_

#include <optional>
#include <string>

#include "base/supports_user_data.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
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
      std::optional<policy::DlpFileDestination> dialog_caller);
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
  static std::optional<policy::DlpFileDestination>
  GetDialogCallerForWebContents(content::WebContents* web_contents);
  // Sets the fake dialog caller value to be used as return value for
  // `GetDialogCallerForWebContents()` calls in tests.
  static void SetDialogCallerForTesting(
      policy::DlpFileDestination* dialog_caller);

 private:
  SelectFileDialogExtensionUserData(
      const std::string& routing_id,
      ui::SelectFileDialog::Type type,
      std::optional<policy::DlpFileDestination> dialog_caller);

  const std::string& routing_id() const { return routing_id_; }

  ui::SelectFileDialog::Type type() const { return type_; }

  std::optional<policy::DlpFileDestination> dialog_caller() const {
    return dialog_caller_;
  }

  std::string routing_id_;
  ui::SelectFileDialog::Type type_;
  std::optional<policy::DlpFileDestination> dialog_caller_;
};

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SELECT_FILE_DIALOG_EXTENSION_USER_DATA_H_
