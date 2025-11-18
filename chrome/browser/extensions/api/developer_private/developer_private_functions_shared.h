// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "extensions/browser/extension_creator.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions::api {

class DeveloperPrivateAPIFunction : public ExtensionFunction {
 protected:
  // These constants here are only temporarily during Android desktop
  // development and we can move these constants to an anonymous namespace once
  // we finish it.
  static constexpr char kNoSuchExtensionError[] =
      "No such extension found for call to '*'.";
  static constexpr char kRequiresUserGestureError[] =
      "This action requires a user gesture.";
  static constexpr char kCouldNotShowSelectFileDialogError[] =
      "Could not show a file chooser.";
  static constexpr char kFileSelectionCanceled[] =
      "File selection was canceled.";
  static constexpr char kNoSuchRendererError[] = "No such renderer.";
  static constexpr char kInvalidPathError[] = "Invalid path.";
  static constexpr char kManifestKeyIsRequiredError[] =
      "The 'manifestKey' argument is required for manifest files.";
  static constexpr char kCouldNotFindWebContentsError[] =
      "Could not find a valid web contents.";
  static constexpr char kNoOptionsPageForExtensionError[] =
      "Extension does not have an options page.";
  static constexpr char kCannotRepairHealthyExtension[] =
      "Cannot repair a healthy extension.";
  static constexpr char kCannotRepairPolicyExtension[] =
      "Cannot repair a policy-installed extension.";
  static constexpr char kCannotChangeHostPermissions[] =
      "Cannot change host permissions for the given extension.";
  static constexpr char kCannotSetPinnedWithoutAction[] =
      "Cannot set pinned action state for an extension with no action.";
  static constexpr char kInvalidHost[] = "Invalid host.";
  static constexpr char kInvalidLazyBackgroundPageParameter[] =
      "isServiceWorker can not be set for lazy background page based "
      "extensions.";
  static constexpr char kInvalidRenderProcessId[] =
      "render_process_id can be set to -1 for only lazy background page based "
      "or "
      "service-worker based extensions.";
  static constexpr char kFailToUninstallEnterpriseOrComponentExtensions[] =
      "Cannot uninstall the enterprise or component extensions in your list.";
  static constexpr char kFailToUninstallNoneExistentExtensions[] =
      "Cannot uninstall non-existent extensions in your list.";
  static constexpr char kUserCancelledError[] = "User cancelled uninstall";
  static constexpr char kNoExtensionError[] =
      "Extension with ID '*' doesn't exist.";
  static constexpr char kExtensionNotAffectedByMV2Deprecation[] =
      "Extension with ID '*' is not affected by the MV2 deprecation.";
  static constexpr char kCannotRepairNonWebstoreExtension[] =
      "Cannot repair an extension that is not installed from the Chrome Web "
      "Store.";
  static constexpr char kCannotDismissExtensionOnUnsupportedStage[] =
      "Cannot dismiss the MV2 deprecation notice for extension with ID '*' on "
      "the unsupported stage.";
  static constexpr char kUserNotSignedIn[] = "User is not signed in.";
  static constexpr char kCannotUploadExtensionToAccount[] =
      "Extension with ID '*' cannot be uploaded to the user's account.";

  static constexpr char kManifestFile[] = "manifest.json";

  // Following helpers are temporarily here during migration for desktop
  // android. We should move them back to anonymous namespace after the
  // migration.
  using GetManifestErrorCallback =
      base::OnceCallback<void(const base::FilePath& file_path,
                              const std::u16string& error,
                              size_t line_number,
                              const std::string& manifest)>;
  // Takes in an `error` string and tries to parse it as a manifest error (with
  // line number), asynchronously calling `callback` with the results.
  void GetManifestError(const std::u16string& error,
                        const base::FilePath& extension_path,
                        GetManifestErrorCallback callback);

  // Creates a developer::LoadError from the provided data.
  developer_private::LoadError CreateLoadError(
      const base::FilePath& file_path,
      const std::u16string& error,
      size_t line_number,
      const std::string& manifest,
      const DeveloperPrivateAPI::UnpackedRetryId& retry_guid);

  ~DeveloperPrivateAPIFunction() override;

  // Returns the extension with the given `id` from the registry, including
  // all possible extensions (enabled, disabled, terminated, etc).
  const Extension* GetExtensionById(const ExtensionId& id);

  // Returns the extension with the given `id` from the registry, only checking
  // enabled extensions.
  const Extension* GetEnabledExtensionById(const ExtensionId& id);

  // Called when there is no extension that exists for a specified ID in a
  // function. Logs the function name and returns an error.
  ResponseValue LogNoSuchExtensionFoundAndReturn();
};

class DeveloperPrivateAutoUpdateFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.autoUpdate",
                             DEVELOPERPRIVATE_AUTOUPDATE)

 protected:
  ~DeveloperPrivateAutoUpdateFunction() override;
  ResponseAction Run() override;

 private:
  void OnComplete();
};

class DeveloperPrivateGetExtensionsInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionsInfoFunction();

  DeveloperPrivateGetExtensionsInfoFunction(
      const DeveloperPrivateGetExtensionsInfoFunction&) = delete;
  DeveloperPrivateGetExtensionsInfoFunction& operator=(
      const DeveloperPrivateGetExtensionsInfoFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionsInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONSINFO)

 private:
  ~DeveloperPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;
};

class DeveloperPrivateGetExtensionInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionInfoFunction();

  DeveloperPrivateGetExtensionInfoFunction(
      const DeveloperPrivateGetExtensionInfoFunction&) = delete;
  DeveloperPrivateGetExtensionInfoFunction& operator=(
      const DeveloperPrivateGetExtensionInfoFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONINFO)

 private:
  ~DeveloperPrivateGetExtensionInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;
};

class DeveloperPrivateGetExtensionSizeFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionSizeFunction();

  DeveloperPrivateGetExtensionSizeFunction(
      const DeveloperPrivateGetExtensionSizeFunction&) = delete;
  DeveloperPrivateGetExtensionSizeFunction& operator=(
      const DeveloperPrivateGetExtensionSizeFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionSize",
                             DEVELOPERPRIVATE_GETEXTENSIONSIZE)

 private:
  ~DeveloperPrivateGetExtensionSizeFunction() override;
  ResponseAction Run() override;

  void OnSizeCalculated(const std::u16string& size);
};

class DeveloperPrivateGetProfileConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getProfileConfiguration",
                             DEVELOPERPRIVATE_GETPROFILECONFIGURATION)

 private:
  ~DeveloperPrivateGetProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateProfileConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateProfileConfiguration",
                             DEVELOPERPRIVATE_UPDATEPROFILECONFIGURATION)

 private:
  ~DeveloperPrivateUpdateProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateExtensionConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateExtensionConfiguration",
                             DEVELOPERPRIVATE_UPDATEEXTENSIONCONFIGURATION)

 protected:
  ~DeveloperPrivateUpdateExtensionConfigurationFunction() override;
  ResponseAction Run() override;
};

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
                     const std::u16string& error) override;

 protected:
  ~DeveloperPrivateReloadFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  // Callback once we parse a manifest error from a failed reload.
  void OnGotManifestError(const base::FilePath& file_path,
                          const std::u16string& error,
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
                      const std::u16string& error);

  // Called when `file_path` load encounters a manifest parsing `error`.
  void OnGotManifestError(const base::FilePath& file_path,
                          const std::u16string& error,
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

class DeveloperPrivateInstallDroppedFileFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.installDroppedFile",
                             DEVELOPERPRIVATE_INSTALLDROPPEDFILE)
  DeveloperPrivateInstallDroppedFileFunction();

  DeveloperPrivateInstallDroppedFileFunction(
      const DeveloperPrivateInstallDroppedFileFunction&) = delete;
  DeveloperPrivateInstallDroppedFileFunction& operator=(
      const DeveloperPrivateInstallDroppedFileFunction&) = delete;

 private:
  ~DeveloperPrivateInstallDroppedFileFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DeveloperPrivateNotifyDragInstallInProgressFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.notifyDragInstallInProgress",
                             DEVELOPERPRIVATE_NOTIFYDRAGINSTALLINPROGRESS)

  DeveloperPrivateNotifyDragInstallInProgressFunction();

  DeveloperPrivateNotifyDragInstallInProgressFunction(
      const DeveloperPrivateNotifyDragInstallInProgressFunction&) = delete;
  DeveloperPrivateNotifyDragInstallInProgressFunction& operator=(
      const DeveloperPrivateNotifyDragInstallInProgressFunction&) = delete;

  ResponseAction Run() override;

  static void SetDropFileForTesting(ui::FileInfo* file_info);

 private:
  ~DeveloperPrivateNotifyDragInstallInProgressFunction() override;
};

class DeveloperPrivateDeleteExtensionErrorsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.deleteExtensionErrors",
                             DEVELOPERPRIVATE_DELETEEXTENSIONERRORS)

 protected:
  ~DeveloperPrivateDeleteExtensionErrorsFunction() override;
  ResponseAction Run() override;
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

class DeveloperPrivateUpdateExtensionCommandFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateExtensionCommand",
                             DEVELOPERPRIVATE_UPDATEEXTENSIONCOMMAND)

 protected:
  ~DeveloperPrivateUpdateExtensionCommandFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateAddHostPermissionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.addHostPermission",
                             DEVELOPERPRIVATE_ADDHOSTPERMISSION)
  DeveloperPrivateAddHostPermissionFunction();

  DeveloperPrivateAddHostPermissionFunction(
      const DeveloperPrivateAddHostPermissionFunction&) = delete;
  DeveloperPrivateAddHostPermissionFunction& operator=(
      const DeveloperPrivateAddHostPermissionFunction&) = delete;

 private:
  ~DeveloperPrivateAddHostPermissionFunction() override;

  ResponseAction Run() override;

  void OnRuntimePermissionsGranted();
};

class DeveloperPrivateRemoveHostPermissionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.removeHostPermission",
                             DEVELOPERPRIVATE_REMOVEHOSTPERMISSION)
  DeveloperPrivateRemoveHostPermissionFunction();

  DeveloperPrivateRemoveHostPermissionFunction(
      const DeveloperPrivateRemoveHostPermissionFunction&) = delete;
  DeveloperPrivateRemoveHostPermissionFunction& operator=(
      const DeveloperPrivateRemoveHostPermissionFunction&) = delete;

 private:
  ~DeveloperPrivateRemoveHostPermissionFunction() override;

  ResponseAction Run() override;

  void OnRuntimePermissionsRevoked();
};

class DeveloperPrivateGetUserSiteSettingsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getUserSiteSettings",
                             DEVELOPERPRIVATE_GETUSERSITESETTINGS)
  DeveloperPrivateGetUserSiteSettingsFunction();

  DeveloperPrivateGetUserSiteSettingsFunction(
      const DeveloperPrivateGetUserSiteSettingsFunction&) = delete;
  DeveloperPrivateGetUserSiteSettingsFunction& operator=(
      const DeveloperPrivateGetUserSiteSettingsFunction&) = delete;

 private:
  ~DeveloperPrivateGetUserSiteSettingsFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateAddUserSpecifiedSitesFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.addUserSpecifiedSites",
                             DEVELOPERPRIVATE_ADDUSERSPECIFIEDSITES)
  DeveloperPrivateAddUserSpecifiedSitesFunction();

  DeveloperPrivateAddUserSpecifiedSitesFunction(
      const DeveloperPrivateAddUserSpecifiedSitesFunction&) = delete;
  DeveloperPrivateAddUserSpecifiedSitesFunction& operator=(
      const DeveloperPrivateAddUserSpecifiedSitesFunction&) = delete;

 private:
  ~DeveloperPrivateAddUserSpecifiedSitesFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateRemoveUserSpecifiedSitesFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.removeUserSpecifiedSites",
                             DEVELOPERPRIVATE_REMOVEUSERSPECIFIEDSITES)
  DeveloperPrivateRemoveUserSpecifiedSitesFunction();

  DeveloperPrivateRemoveUserSpecifiedSitesFunction(
      const DeveloperPrivateRemoveUserSpecifiedSitesFunction&) = delete;
  DeveloperPrivateRemoveUserSpecifiedSitesFunction& operator=(
      const DeveloperPrivateRemoveUserSpecifiedSitesFunction&) = delete;

 private:
  ~DeveloperPrivateRemoveUserSpecifiedSitesFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateGetUserAndExtensionSitesByEtldFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getUserAndExtensionSitesByEtld",
                             DEVELOPERPRIVATE_GETUSERANDEXTENSIONSITESBYETLD)
  DeveloperPrivateGetUserAndExtensionSitesByEtldFunction();

  DeveloperPrivateGetUserAndExtensionSitesByEtldFunction(
      const DeveloperPrivateGetUserAndExtensionSitesByEtldFunction&) = delete;
  DeveloperPrivateGetUserAndExtensionSitesByEtldFunction& operator=(
      const DeveloperPrivateGetUserAndExtensionSitesByEtldFunction&) = delete;

 private:
  ~DeveloperPrivateGetUserAndExtensionSitesByEtldFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateGetMatchingExtensionsForSiteFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getMatchingExtensionsForSite",
                             DEVELOPERPRIVATE_GETMATCHINGEXTENSIONSFORSITE)
  DeveloperPrivateGetMatchingExtensionsForSiteFunction();

  DeveloperPrivateGetMatchingExtensionsForSiteFunction(
      const DeveloperPrivateGetMatchingExtensionsForSiteFunction&) = delete;
  DeveloperPrivateGetMatchingExtensionsForSiteFunction& operator=(
      const DeveloperPrivateGetMatchingExtensionsForSiteFunction&) = delete;

 private:
  ~DeveloperPrivateGetMatchingExtensionsForSiteFunction() override;

  ResponseAction Run() override;
};

class DeveloperPrivateUpdateSiteAccessFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateSiteAccess",
                             DEVELOPERPRIVATE_UPDATESITEACCESS)
  DeveloperPrivateUpdateSiteAccessFunction();

  DeveloperPrivateUpdateSiteAccessFunction(
      const DeveloperPrivateUpdateSiteAccessFunction&) = delete;
  DeveloperPrivateUpdateSiteAccessFunction& operator=(
      const DeveloperPrivateUpdateSiteAccessFunction&) = delete;

 private:
  ~DeveloperPrivateUpdateSiteAccessFunction() override;

  ResponseAction Run() override;

  void OnSiteSettingsUpdated();
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

class DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "developerPrivate.dismissSafetyHubExtensionsMenuNotification",
      DEVELOPERPRIVATE_DISMISSSAFETYHUBEXTENSIONSMENUNOTIFICATION)
  DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction();

  DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction(
      const DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction&) =
      delete;
  DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction& operator=(
      const DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction&) =
      delete;

  ResponseAction Run() override;

 private:
  ~DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction()
      override;
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

class DeveloperPrivateChoosePathFunction
    : public DeveloperPrivateAPIFunction,
      public ui::SelectFileDialog::Listener {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.choosePath",
                             DEVELOPERPRIVATE_CHOOSEPATH)
  DeveloperPrivateChoosePathFunction();

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
  ~DeveloperPrivateChoosePathFunction() override;
  ResponseAction Run() override;

 private:
  // The dialog with the select file picker.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // For testing:
  // Whether to accept or reject the select file dialog without showing it.
  std::optional<bool> accept_dialog_for_testing_;
  // File to load when accepting the select file dialog without showing it.
  std::optional<ui::SelectedFileInfo> selected_file_for_testing_;
};

class DeveloperPrivateRequestFileSourceFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.requestFileSource",
                             DEVELOPERPRIVATE_REQUESTFILESOURCE)
  DeveloperPrivateRequestFileSourceFunction();

 protected:
  ~DeveloperPrivateRequestFileSourceFunction() override;
  ResponseAction Run() override;

 private:
  void Finish(const std::string& file_contents);

  std::optional<api::developer_private::RequestFileSource::Params> params_;
};

class DeveloperPrivateOpenDevToolsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.openDevTools",
                             DEVELOPERPRIVATE_OPENDEVTOOLS)
  DeveloperPrivateOpenDevToolsFunction();

 protected:
  ~DeveloperPrivateOpenDevToolsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateRepairExtensionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.repairExtension",
                             DEVELOPERPRIVATE_REPAIREXTENSION)

 protected:
  ~DeveloperPrivateRepairExtensionFunction() override;
  ResponseAction Run() override;

  void OnReinstallComplete(bool success,
                           const std::string& error,
                           webstore_install::Result result);
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

}  // namespace extensions::api

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
