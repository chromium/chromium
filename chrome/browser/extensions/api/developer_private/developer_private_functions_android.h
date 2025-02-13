// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_ANDROID_H_

#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/extension_function.h"

#define DECLARE_EMPTY_FUNC(class_name, api_name, histogram_value) \
  class class_name : public ExtensionFunction {                   \
   public:                                                        \
    DECLARE_EXTENSION_FUNCTION(api_name, histogram_value)         \
   protected:                                                     \
    ~class_name() override;                                       \
    ResponseAction Run() override;                                \
  }

DECLARE_EMPTY_FUNC(DeveloperPrivateAutoUpdateFunction,
                   "developerPrivate.autoUpdate",
                   DEVELOPERPRIVATE_AUTOUPDATE);
DECLARE_EMPTY_FUNC(DeveloperPrivateGetExtensionsInfoFunction,
                   "developerPrivate.getExtensionsInfo",
                   DEVELOPERPRIVATE_GETEXTENSIONSINFO);
DECLARE_EMPTY_FUNC(DeveloperPrivateGetExtensionInfoFunction,
                   "developerPrivate.getExtensionInfo",
                   DEVELOPERPRIVATE_GETEXTENSIONINFO);
DECLARE_EMPTY_FUNC(DeveloperPrivateGetExtensionSizeFunction,
                   "developerPrivate.getExtensionSize",
                   DEVELOPERPRIVATE_GETEXTENSIONSIZE);
DECLARE_EMPTY_FUNC(DeveloperPrivateGetProfileConfigurationFunction,
                   "developerPrivate.getProfileConfiguration",
                   DEVELOPERPRIVATE_GETPROFILECONFIGURATION);
DECLARE_EMPTY_FUNC(DeveloperPrivateUpdateProfileConfigurationFunction,
                   "developerPrivate.updateProfileConfiguration",
                   DEVELOPERPRIVATE_UPDATEPROFILECONFIGURATION);
DECLARE_EMPTY_FUNC(DeveloperPrivateUpdateExtensionConfigurationFunction,
                   "developerPrivate.updateExtensionConfiguration",
                   DEVELOPERPRIVATE_UPDATEEXTENSIONCONFIGURATION);
DECLARE_EMPTY_FUNC(DeveloperPrivateReloadFunction,
                   "developerPrivate.reload",
                   DEVELOPERPRIVATE_RELOAD);
DECLARE_EMPTY_FUNC(DeveloperPrivateLoadUnpackedFunction,
                   "developerPrivate.loadUnpacked",
                   DEVELOPERPRIVATE_LOADUNPACKED);
DECLARE_EMPTY_FUNC(DeveloperPrivateInstallDroppedFileFunction,
                   "developerPrivate.installDroppedFile",
                   DEVELOPERPRIVATE_INSTALLDROPPEDFILE);
DECLARE_EMPTY_FUNC(DeveloperPrivateNotifyDragInstallInProgressFunction,
                   "developerPrivate.notifyDragInstallInProgress",
                   DEVELOPERPRIVATE_NOTIFYDRAGINSTALLINPROGRESS);
DECLARE_EMPTY_FUNC(DeveloperPrivateChoosePathFunction,
                   "developerPrivate.choosePath",
                   DEVELOPERPRIVATE_CHOOSEPATH);
DECLARE_EMPTY_FUNC(DeveloperPrivatePackDirectoryFunction,
                   "developerPrivate.packDirectory",
                   DEVELOPERPRIVATE_PACKDIRECTORY);
DECLARE_EMPTY_FUNC(DeveloperPrivateIsProfileManagedFunction,
                   "developerPrivate.isProfileManaged",
                   DEVELOPERPRIVATE_ISPROFILEMANAGED);
DECLARE_EMPTY_FUNC(DeveloperPrivateLoadDirectoryFunction,
                   "developerPrivate.loadDirectory",
                   DEVELOPERPRIVATE_LOADUNPACKEDCROS);
DECLARE_EMPTY_FUNC(DeveloperPrivateRequestFileSourceFunction,
                   "developerPrivate.requestFileSource",
                   DEVELOPERPRIVATE_REQUESTFILESOURCE);
DECLARE_EMPTY_FUNC(DeveloperPrivateOpenDevToolsFunction,
                   "developerPrivate.openDevTools",
                   DEVELOPERPRIVATE_OPENDEVTOOLS);
DECLARE_EMPTY_FUNC(DeveloperPrivateDeleteExtensionErrorsFunction,
                   "developerPrivate.deleteExtensionErrors",
                   DEVELOPERPRIVATE_DELETEEXTENSIONERRORS);
DECLARE_EMPTY_FUNC(DeveloperPrivateRepairExtensionFunction,
                   "developerPrivate.repairExtension",
                   DEVELOPERPRIVATE_REPAIREXTENSION);
DECLARE_EMPTY_FUNC(DeveloperPrivateShowOptionsFunction,
                   "developerPrivate.showOptions",
                   DEVELOPERPRIVATE_SHOWOPTIONS);
DECLARE_EMPTY_FUNC(DeveloperPrivateShowPathFunction,
                   "developerPrivate.showPath",
                   DEVELOPERPRIVATE_SHOWPATH);
DECLARE_EMPTY_FUNC(DeveloperPrivateSetShortcutHandlingSuspendedFunction,
                   "developerPrivate.setShortcutHandlingSuspended",
                   DEVELOPERPRIVATE_SETSHORTCUTHANDLINGSUSPENDED);
DECLARE_EMPTY_FUNC(DeveloperPrivateUpdateExtensionCommandFunction,
                   "developerPrivate.updateExtensionCommand",
                   DEVELOPERPRIVATE_UPDATEEXTENSIONCOMMAND);
DECLARE_EMPTY_FUNC(DeveloperPrivateAddHostPermissionFunction,
                   "developerPrivate.addHostPermission",
                   DEVELOPERPRIVATE_ADDHOSTPERMISSION);
DECLARE_EMPTY_FUNC(DeveloperPrivateRemoveHostPermissionFunction,
                   "developerPrivate.removeHostPermission",
                   DEVELOPERPRIVATE_REMOVEHOSTPERMISSION);
DECLARE_EMPTY_FUNC(DeveloperPrivateGetUserSiteSettingsFunction,
                   "developerPrivate.getUserSiteSettings",
                   DEVELOPERPRIVATE_GETUSERSITESETTINGS);
DECLARE_EMPTY_FUNC(DeveloperPrivateAddUserSpecifiedSitesFunction,
                   "developerPrivate.addUserSpecifiedSites",
                   DEVELOPERPRIVATE_ADDUSERSPECIFIEDSITES);
DECLARE_EMPTY_FUNC(DeveloperPrivateRemoveUserSpecifiedSitesFunction,
                   "developerPrivate.removeUserSpecifiedSites",
                   DEVELOPERPRIVATE_REMOVEUSERSPECIFIEDSITES);
DECLARE_EMPTY_FUNC(DeveloperPrivateGetUserAndExtensionSitesByEtldFunction,
                   "developerPrivate.getUserAndExtensionSitesByEtld",
                   DEVELOPERPRIVATE_GETUSERANDEXTENSIONSITESBYETLD);
DECLARE_EMPTY_FUNC(DeveloperPrivateGetMatchingExtensionsForSiteFunction,
                   "developerPrivate.getMatchingExtensionsForSite",
                   DEVELOPERPRIVATE_GETMATCHINGEXTENSIONSFORSITE);
DECLARE_EMPTY_FUNC(DeveloperPrivateUpdateSiteAccessFunction,
                   "developerPrivate.updateSiteAccess",
                   DEVELOPERPRIVATE_UPDATESITEACCESS);
DECLARE_EMPTY_FUNC(DeveloperPrivateRemoveMultipleExtensionsFunction,
                   "developerPrivate.removeMultipleExtensions",
                   DEVELOPERPRIVATE_REMOVEMULTIPLEEXTENSIONS);
DECLARE_EMPTY_FUNC(
    DeveloperPrivateDismissSafetyHubExtensionsMenuNotificationFunction,
    "developerPrivate.dismissSafetyHubExtensionsMenuNotification",
    DEVELOPERPRIVATE_DISMISSSAFETYHUBEXTENSIONSMENUNOTIFICATION);
DECLARE_EMPTY_FUNC(
    DeveloperPrivateDismissMv2DeprecationNoticeForExtensionFunction,
    "developerPrivate.dismissMv2DeprecationNoticeForExtension",
    DEVELOPERPRIVATE_DISMISSMV2DEPRECATIONNOTICEFOREXTENSION);
DECLARE_EMPTY_FUNC(DeveloperPrivateUploadExtensionToAccountFunction,
                   "developerPrivate.uploadExtensionToAccount",
                   DEVELOPERPRIVATE_UPLOADEXTENSIONTOACCOUNT);

#undef DECLARE_EMPTY_FUNC

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_FUNCTIONS_ANDROID_H_
