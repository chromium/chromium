// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNING_SCANNING_HANDLER_H_
#define ASH_WEBUI_SCANNING_SCANNING_HANDLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {
class WebUI;
}  // namespace content

namespace ash {

class ScanningAppDelegate;

// ChromeOS Scanning app UI handler.
class ScanningHandler : public content::WebUIMessageHandler,
                        public ui::SelectFileDialog::Listener {
 public:
  explicit ScanningHandler(
      std::unique_ptr<ScanningAppDelegate> scanning_app_delegate);
  ~ScanningHandler() override;

  ScanningHandler(const ScanningHandler&) = delete;
  ScanningHandler& operator=(const ScanningHandler&) = delete;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // Uses the full filepath and the base directory (lowest level directory in
  // the filepath, used to display in the UI) to create a Value object to return
  // to the Scanning UI.
  base::Value::Dict CreateSelectedPathValue(const base::FilePath& path);

  // Adds to map of string IDs for pluralization.
  void AddStringToPluralMap(const std::string& name, int id);

  void SetWebUIForTest(content::WebUI* web_ui);

 private:
  // Initializes Javascript.
  void HandleInitialize(const base::Value::List& args);

  // Opens the select dialog for the user to choose the directory to save
  // completed scans.
  void HandleRequestScanToLocation(const base::Value::List& args);

  // Opens the Media app with the completed scan files.
  void HandleOpenFilesInMediaApp(const base::Value::List& args);

  // Opens the Files app to the show the saved scan file.
  void HandleShowFileInLocation(const base::Value::List& args);

  // Callback for HandleShowFileInLocation().
  void OnShowFileInLocation(const std::string& callback, bool files_app_opened);

  // Returns a localized, pluralized string.
  void HandleGetPluralString(const base::Value::List& args);

  // Gets the MyFiles path for the current user.
  void HandleGetMyFilesPath(const base::Value::List& args);

  // Saves scan settings to Pref service.
  void HandleSaveScanSettings(const base::Value::List& args);

  // Fetches scan settings from Pref service.
  void HandleGetScanSettings(const base::Value::List& args);

  // Validates that a file path exists on the local filesystem and returns its
  // display name. If the file path doesn't exist, return an empty file path.
  void HandleEnsureValidFilePath(const base::Value::List& args);

  // Callback for HandleEnsureValidFilePath().
  void OnPathExists(const base::FilePath& file_path,
                    const std::string& callback,
                    bool file_path_exists);

  std::string scan_location_callback_id_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // Provides browser functionality from //chrome to the Scan app UI.
  std::unique_ptr<ScanningAppDelegate> scanning_app_delegate_;

  std::map<std::string, int> string_id_map_;

  // Task runner for the I/O function base::FilePath().
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<ScanningHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_SCANNING_SCANNING_HANDLER_H_
