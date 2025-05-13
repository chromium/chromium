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
#include "chrome/browser/extensions/load_error_reporter.h"
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

class DeveloperPrivateReloadFunction : public DeveloperPrivateAPIFunction,
                                       public ExtensionRegistryObserver,
                                       public LoadErrorReporter::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload", DEVELOPERPRIVATE_RELOAD)

  DeveloperPrivateReloadFunction();

  DeveloperPrivateReloadFunction(const DeveloperPrivateReloadFunction&) =
      delete;
  DeveloperPrivateReloadFunction& operator=(
      const DeveloperPrivateReloadFunction&) = delete;

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnShutdown(ExtensionRegistry* registry) override;

  // LoadErrorReporter::Observer:
  void OnLoadFailure(content::BrowserContext* browser_context,
                     const base::FilePath& file_path,
                     const std::string& error) override;

 protected:
  ~DeveloperPrivateReloadFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Callback once we parse a manifest error from a failed reload.
  void OnGotManifestError(const base::FilePath& file_path,
                          const std::string& error,
                          size_t line_number,
                          const std::string& manifest);

  // Clears the scoped observers.
  void ClearObservers();

  // The file path of the extension that's reloading.
  base::FilePath reloading_extension_path_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
  base::ScopedObservation<LoadErrorReporter, LoadErrorReporter::Observer>
      error_reporter_observation_{this};
};

class DeveloperPrivateLoadUnpackedFunction
    : public DeveloperPrivateAPIFunction,
      public ui::SelectFileDialog::Listener {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadUnpacked",
                             DEVELOPERPRIVATE_LOADUNPACKED)
  DeveloperPrivateLoadUnpackedFunction();

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // For testing:
  void set_accept_dialog_for_testing(bool accept) {
    accept_dialog_for_testing_ = accept;
  }
  void set_selected_file_for_testing(const ui::SelectedFileInfo& file) {
    selected_file_for_testing_ = file;
  }

 protected:
  ~DeveloperPrivateLoadUnpackedFunction() override;

  // DeveloperPrivateAPIFunction:
  ResponseAction Run() override;

 private:
  // Shows the file picker dialog.
  void ShowSelectFileDialog();

  // Starts loading the given `file_path`.
  void StartFileLoad(const base::FilePath file_path);

  // Called when `file_path` load is completed
  void OnLoadComplete(const Extension* extension,
                      const base::FilePath& file_path,
                      const std::string& error);

  // Called when `file_path` load encounters a manifest parsing `error`.
  void OnGotManifestError(const base::FilePath& file_path,
                          const std::string& error,
                          size_t line_number,
                          const std::string& manifest);

  // Returns `response_value` when the function should finish asynchronously.
  void Finish(ResponseValue response_value);

  // Whether or not we should fail quietly in the event of a load error.
  bool fail_quietly_ = false;

  // Whether we populate a developer_private::LoadError on load failure, as
  // opposed to simply passing the message in lastError.
  bool populate_error_ = false;

  // The identifier for the selected path when retrying an unpacked load.
  DeveloperPrivateAPI::UnpackedRetryId retry_guid_;

  // The dialog with the select file picker.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // For testing:
  // Whether to accept or reject the select file dialog without showing it.
  std::optional<bool> accept_dialog_for_testing_;
  // File to load when accepting the select file dialog without showing it.
  std::optional<ui::SelectedFileInfo> selected_file_for_testing_;
};

class DeveloperPrivatePackDirectoryFunction
    : public DeveloperPrivateAPIFunction,
      public PackExtensionJob::Client {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.packDirectory",
                             DEVELOPERPRIVATE_PACKDIRECTORY)

  DeveloperPrivatePackDirectoryFunction();

  // ExtensionPackJob::Client implementation.
  void OnPackSuccess(const base::FilePath& crx_file,
                     const base::FilePath& key_file) override;
  void OnPackFailure(const std::string& error,
                     ExtensionCreator::ErrorType error_type) override;

 protected:
  ~DeveloperPrivatePackDirectoryFunction() override;
  ResponseAction Run() override;

 private:
  std::unique_ptr<PackExtensionJob> pack_job_;
  std::string item_path_str_;
  std::string key_path_str_;
};

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

class DeveloperPrivateShowOptionsFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showOptions",
                             DEVELOPERPRIVATE_SHOWOPTIONS)

 protected:
  ~DeveloperPrivateShowOptionsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateShowPathFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPath",
                             DEVELOPERPRIVATE_SHOWPATH)

 protected:
  ~DeveloperPrivateShowPathFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateSetShortcutHandlingSuspendedFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.setShortcutHandlingSuspended",
                             DEVELOPERPRIVATE_SETSHORTCUTHANDLINGSUSPENDED)

 protected:
  ~DeveloperPrivateSetShortcutHandlingSuspendedFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateRemoveMultipleExtensionsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.removeMultipleExtensions",
                             DEVELOPERPRIVATE_REMOVEMULTIPLEEXTENSIONS)
  DeveloperPrivateRemoveMultipleExtensionsFunction();

  DeveloperPrivateRemoveMultipleExtensionsFunction(
      const DeveloperPrivateRemoveMultipleExtensionsFunction&) = delete;
  DeveloperPrivateRemoveMultipleExtensionsFunction& operator=(
      const DeveloperPrivateRemoveMultipleExtensionsFunction&) = delete;

  void accept_bubble_for_testing(bool accept_bubble) {
    accept_bubble_for_testing_ = accept_bubble;
  }

 private:
  ~DeveloperPrivateRemoveMultipleExtensionsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // A callback function to run when the user accepts the action dialog.
  void OnDialogAccepted();

  // A callback function to run when the user cancels the action dialog.
  void OnDialogCancelled();

  // The IDs of the extensions to be uninstalled.
  std::vector<ExtensionId> extension_ids_;

  raw_ptr<Profile> profile_;

  // If true, immediately accept the blocked action dialog by running the
  // callback.
  std::optional<bool> accept_bubble_for_testing_;
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

class DeveloperPrivateUploadExtensionToAccountFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.uploadExtensionToAccount",
                             DEVELOPERPRIVATE_UPLOADEXTENSIONTOACCOUNT)
  DeveloperPrivateUploadExtensionToAccountFunction();

  DeveloperPrivateUploadExtensionToAccountFunction(
      const DeveloperPrivateUploadExtensionToAccountFunction&) = delete;
  DeveloperPrivateUploadExtensionToAccountFunction& operator=(
      const DeveloperPrivateUploadExtensionToAccountFunction&) = delete;

  void accept_bubble_for_testing(bool accept_bubble) {
    accept_bubble_for_testing_ = accept_bubble;
  }

 private:
  ~DeveloperPrivateUploadExtensionToAccountFunction() override;

  ResponseAction Run() override;

  // Verify that the extension to be uploaded exists and that there's a signed
  // in user. Returns the extension if successful, otherwise returns an error.
  base::expected<const Extension*, std::string> VerifyExtensionAndSigninState();

  // Uploads the given `extension` to the user's account.
  void UploadExtensionToAccount(const Extension& extension);

  // A callback function to run when the user accepts the action dialog.
  void OnDialogAccepted();

  // A callback function to run when the user cancels the action dialog.
  void OnDialogCancelled();

  // The ID of the extension to be uploaded.
  ExtensionId extension_id_;

  raw_ptr<Profile> profile_;

  // If true, immediately accepts the keep dialog by running the callback.
  std::optional<bool> accept_bubble_for_testing_;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_DESKTOP_H_
