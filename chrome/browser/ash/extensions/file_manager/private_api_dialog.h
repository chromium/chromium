// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides API functions for the file manager to act as the file
// dialog for opening and saving files.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DIALOG_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DIALOG_H_

#include <vector>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"
#include "chrome/browser/ash/extensions/file_manager/logged_extension_function.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

namespace ui {
struct SelectedFileInfo;
}

namespace extensions {

// Cancel file selection Dialog.  Closes the dialog window.
class FileManagerPrivateCancelDialogFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.cancelDialog",
                             FILEMANAGERPRIVATE_CANCELDIALOG)

 protected:
  ~FileManagerPrivateCancelDialogFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

class FileManagerPrivateSelectFileFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.selectFile",
                             FILEMANAGERPRIVATE_SELECTFILE)

 protected:
  ~FileManagerPrivateSelectFileFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(
      bool for_open,
      int index,
      const std::vector<ui::SelectedFileInfo>& files);
};

// Select multiple files.  Closes the dialog window.
class FileManagerPrivateSelectFilesFunction : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.selectFiles",
                             FILEMANAGERPRIVATE_SELECTFILES)

  FileManagerPrivateSelectFilesFunction();

 protected:
  ~FileManagerPrivateSelectFilesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  void OnReSyncFile();

  // A callback method to handle the result of GetSelectedFileInfo.
  void GetSelectedFileInfoResponse(
      bool for_open,
      const std::vector<ui::SelectedFileInfo>& files);

  bool should_return_local_path_;
  // Only used when we need to resync files so we can save the local paths of
  // the selected files after resync is done.
  std::vector<base::FilePath> local_paths_for_resync_callback_;
  int resync_files_remaining_ = 0;
};

// Get a list of Android picker apps.
class FileManagerPrivateGetAndroidPickerAppsFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.getAndroidPickerApps",
                             FILEMANAGERPRIVATE_GETANDROIDPICKERAPPS)

 protected:
  ~FileManagerPrivateGetAndroidPickerAppsFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  void OnActivitiesLoaded(
      std::vector<arc::mojom::IntentHandlerInfoPtr> handlers);

  void OnIconsLoaded(
      std::vector<arc::mojom::IntentHandlerInfoPtr> handlers,
      std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons);
};

// Select an Android picker app.  Closes the dialog window.
class FileManagerPrivateSelectAndroidPickerAppFunction
    : public LoggedExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileManagerPrivate.selectAndroidPickerApp",
                             FILEMANAGERPRIVATE_SELECTANDROIDPICKERAPP)

 protected:
  ~FileManagerPrivateSelectAndroidPickerAppFunction() override = default;

  // ExtensionFunction overrides.
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_PRIVATE_API_DIALOG_H_
