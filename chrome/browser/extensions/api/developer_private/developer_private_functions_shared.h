// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace extensions::api {

class DeveloperPrivateAPIFunction : public ExtensionFunction {
 protected:
  // These constants here are only temporarily during Android desktop
  // development and we can move these constants to an anonymous namespace once
  // we finish it.
  static constexpr char kNoSuchExtensionError[] = "No such extension.";
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

  ~DeveloperPrivateAPIFunction() override;

  // Returns the extension with the given `id` from the registry, including
  // all possible extensions (enabled, disabled, terminated, etc).
  const Extension* GetExtensionById(const ExtensionId& id);

  // Returns the extension with the given `id` from the registry, only checking
  // enabled extensions.
  const Extension* GetEnabledExtensionById(const ExtensionId& id);
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

}  // namespace extensions::api

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
