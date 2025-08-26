// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_DESKTOP_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_DESKTOP_H_

#include <map>
#include <optional>
#include <set>

#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_event_router.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_functions_shared.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/api/file_system/file_system_api.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_id.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

class Profile;

namespace extensions {

namespace api {

class DeveloperPrivateLoadDirectoryFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadDirectory",
                             DEVELOPERPRIVATE_LOADUNPACKEDCROS)

  DeveloperPrivateLoadDirectoryFunction();

 protected:
  ~DeveloperPrivateLoadDirectoryFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  ResponseAction LoadByFileSystemAPI(
      const ::storage::FileSystemURL& directory_url);

  void ClearExistingDirectoryContent(const base::FilePath& project_path);

  void ReadDirectoryByFileSystemAPI(const base::FilePath& project_path,
                                    const base::FilePath& destination_path);

  void ReadDirectoryByFileSystemAPICb(
      const base::FilePath& project_path,
      const base::FilePath& destination_path,
      base::File::Error result,
      ::storage::FileSystemOperation::FileEntryList file_list,
      bool has_more);

  void SnapshotFileCallback(
      const base::FilePath& target_path,
      base::File::Error result,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      scoped_refptr<::storage::ShareableFileReference> file_ref);

  void CopyFile(const base::FilePath& src_path,
                const base::FilePath& dest_path);

  void Load();

  scoped_refptr<::storage::FileSystemContext> context_;

  // syncfs url representing the root of the folder to be copied.
  std::string project_base_url_;

  // physical path on disc of the folder to be copied.
  base::FilePath project_base_path_;

 private:
  int pending_copy_operations_count_;

  // This is set to false if any of the copyFile operations fail on
  // call of the API. It is returned as a response of the API call.
  bool success_;

  // Error string if `success_` is false.
  std::string error_;
};

class DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "developerPrivate.dismissMv2DeprecationNoticeForExtension",
      DEVELOPERPRIVATE_DISMISSMV2DEPRECATIONNOTICEFOREXTENSION)
  DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction();

  DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction(
      const DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction&) =
      delete;
  DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction& operator=(
      const DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction&) =
      delete;

  void accept_bubble_for_testing(bool accept_bubble) {
    accept_bubble_for_testing_ = accept_bubble;
  }

 private:
  ~DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void DismissExtensionNotice();

  // Callback to run when the user accepts the keep dialog.
  void OnDialogAccepted();

  // Callback to run when the user cancels the keep dialog.
  void OnDialogCancelled();

  // The ID of the extension to be dismissed.
  ExtensionId extension_id_;

  // If true, immediately accepts the keep dialog by running the callback.
  std::optional<bool> accept_bubble_for_testing_;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_DESKTOP_H_
