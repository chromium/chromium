// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_functions_android.h"

#include "extensions/browser/extension_function.h"

#define DEFINE_EMPTY_FUNC(class_name, api_name)                        \
  class_name::~class_name() = default;                                 \
  ExtensionFunction::ResponseAction class_name::Run() {                \
    return RespondNow(Error(api_name " not implemented for desktop")); \
  }

DEFINE_EMPTY_FUNC(DeveloperPrivateAutoUpdateFunction,
                  "developerPrivate.autoUpdate")
DEFINE_EMPTY_FUNC(DeveloperPrivateGetExtensionsInfoFunction,
                  "developerPrivate.getExtensionsInfo")
DEFINE_EMPTY_FUNC(DeveloperPrivateGetExtensionInfoFunction,
                  "developerPrivate.getExtensionInfo")
DEFINE_EMPTY_FUNC(DeveloperPrivateGetExtensionSizeFunction,
                  "developerPrivate.getExtensionSize")
DEFINE_EMPTY_FUNC(DeveloperPrivateGetProfileConfigurationFunction,
                  "developerPrivate.getProfileConfiguration")
DEFINE_EMPTY_FUNC(DeveloperPrivateUpdateProfileConfigurationFunction,
                  "developerPrivate.updateProfileConfiguration")
DEFINE_EMPTY_FUNC(DeveloperPrivateUpdateExtensionConfigurationFunction,
                  "developerPrivate.updateExtensionConfiguration")
DEFINE_EMPTY_FUNC(DeveloperPrivateReloadFunction, "developerPrivate.reload")
DEFINE_EMPTY_FUNC(DeveloperPrivateLoadUnpackedFunction,
                  "developerPrivate.loadUnpacked")
DEFINE_EMPTY_FUNC(DeveloperPrivateInstallDroppedFileFunction,
                  "developerPrivate.installDroppedFile")
DEFINE_EMPTY_FUNC(DeveloperPrivateNotifyDragInstallInProgressFunction,
                  "developerPrivate.notifyDragInstallInProgress")
DEFINE_EMPTY_FUNC(DeveloperPrivateChoosePathFunction,
                  "developerPrivate.choosePath")
DEFINE_EMPTY_FUNC(DeveloperPrivatePackDirectoryFunction,
                  "developerPrivate.packDirectory")
DEFINE_EMPTY_FUNC(DeveloperPrivateIsProfileManagedFunction,
                  "developerPrivate.isProfileManaged")
DEFINE_EMPTY_FUNC(DeveloperPrivateLoadDirectoryFunction,
                  "developerPrivate.loadDirectory")
DEFINE_EMPTY_FUNC(DeveloperPrivateRequestFileSourceFunction,
                  "developerPrivate.requestFileSource")
DEFINE_EMPTY_FUNC(DeveloperPrivateOpenDevToolsFunction,
                  "developerPrivate.openDevTools")
DEFINE_EMPTY_FUNC(DeveloperPrivateDeleteExtensionErrorsFunction,
                  "developerPrivate.deleteExtensionErrors")
DEFINE_EMPTY_FUNC(DeveloperPrivateRepairExtensionFunction,
                  "developerPrivate.repairExtension")
DEFINE_EMPTY_FUNC(DeveloperPrivateShowOptionsFunction,
                  "developerPrivate.showOptions")
DEFINE_EMPTY_FUNC(DeveloperPrivateShowPathFunction, "developerPrivate.showPath")
DEFINE_EMPTY_FUNC(DeveloperPrivateSetShortcutHandlingSuspendedFunction,
                  "developerPrivate.setShortcutHandlingSuspended")
DEFINE_EMPTY_FUNC(DeveloperPrivateUpdateExtensionCommandFunction,
                  "developerPrivate.updateExtensionCommand")
DEFINE_EMPTY_FUNC(DeveloperPrivateAddHostPermissionFunction,
                  "developerPrivate.addHostPermission")
DEFINE_EMPTY_FUNC(DeveloperPrivateRemoveHostPermissionFunction,
                  "developerPrivate.removeHostPermission")
DEFINE_EMPTY_FUNC(DeveloperPrivateGetUserSiteSettingsFunction,
                  "developerPrivate.getUserSiteSettings")
DEFINE_EMPTY_FUNC(DeveloperPrivateAddUserSpecifiedSitesFunction,
                  "developerPrivate.addUserSpecifiedSites")
DEFINE_EMPTY_FUNC(DeveloperPrivateRemoveUserSpecifiedSitesFunction,
                  "developerPrivate.removeUserSpecifiedSites")
DEFINE_EMPTY_FUNC(DeveloperPrivateGetUserAndExtensionSitesByEtldFunction,
                  "developerPrivate.getUserAndExtensionSitesByEtld")
DEFINE_EMPTY_FUNC(DeveloperPrivateGetMatchingExtensionsForSiteFunction,
                  "developerPrivate.getMatchingExtensionsForSite")
DEFINE_EMPTY_FUNC(DeveloperPrivateUpdateSiteAccessFunction,
                  "developerPrivate.updateSiteAccess")
DEFINE_EMPTY_FUNC(DeveloperPrivateRemoveMultipleExtensionsFunction,
                  "developerPrivate.removeMultipleExtensions")
DEFINE_EMPTY_FUNC(
    DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction,
    "developerPrivate.dismissSafetyHubExtensionsMenuNotification")
DEFINE_EMPTY_FUNC(
    DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction,
    "developerPrivate.dismissMv2DeprecationNoticeForExtension")
DEFINE_EMPTY_FUNC(DeveloperPrivateUploadExtensionToAccountFunction,
                  "developerPrivate.uploadExtensionToAccount")
