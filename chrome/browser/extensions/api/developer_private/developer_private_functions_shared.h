// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_

#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"

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

  // Returns the extension with the given |id| from the registry, including
  // all possible extensions (enabled, disabled, terminated, etc).
  const Extension* GetExtensionById(const ExtensionId& id);

  // Returns the extension with the given |id| from the registry, only checking
  // enabled extensions.
  const Extension* GetEnabledExtensionById(const ExtensionId& id);
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

}  // namespace extensions::api

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_SHARED_H_
