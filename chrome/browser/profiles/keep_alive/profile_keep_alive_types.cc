// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"

#include "base/notreached.h"

std::ostream& operator<<(std::ostream& out,
                         const ProfileKeepAliveOrigin& origin) {
  switch (origin) {
    case ProfileKeepAliveOrigin::kWaitingForFirstBrowserWindow:
      return out << "kWaitingForFirstBrowserWindow";
    case ProfileKeepAliveOrigin::kBrowserWindow:
      return out << "kBrowserWindow";
    case ProfileKeepAliveOrigin::kBackgroundMode:
      return out << "kBackgroundMode";
    case ProfileKeepAliveOrigin::kOffTheRecordProfile:
      return out << "kOffTheRecordProfile";
    case ProfileKeepAliveOrigin::kDownloadInProgress:
      return out << "kDownloadInProgress";
    case ProfileKeepAliveOrigin::kClearingBrowsingData:
      return out << "kClearingBrowsingData";
    case ProfileKeepAliveOrigin::kAppWindow:
      return out << "kAppWindow";
    case ProfileKeepAliveOrigin::kBackgroundSync:
      return out << "kBackgroundSync";
    case ProfileKeepAliveOrigin::kNotification:
      return out << "kNotification";
    case ProfileKeepAliveOrigin::kPendingNotificationClickEvent:
      return out << "kPendingNotificationClickEvent";
    case ProfileKeepAliveOrigin::kInFlightPushMessage:
      return out << "kInFlightPushMessage";
    case ProfileKeepAliveOrigin::kSessionRestore:
      return out << "kSessionRestore";
    case ProfileKeepAliveOrigin::kChromeViewsDelegate:
      return out << "kChromeViewsDelegate";
    case ProfileKeepAliveOrigin::kDevToolsWindow:
      return out << "kDevToolsWindow";
    case ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow:
      return out << "kWebAppPermissionDialogWindow";
    case ProfileKeepAliveOrigin::kSessionDataDeleter:
      return out << "kSessionDataDeleter";
    case ProfileKeepAliveOrigin::kExtensionUpdater:
      return out << "kExtensionUpdater";
    case ProfileKeepAliveOrigin::kProfileCreationFlow:
      return out << "kProfileCreationFlow";
    case ProfileKeepAliveOrigin::kPendingNotificationCloseEvent:
      return out << "kPendingNotificationCloseEvent";
    case ProfileKeepAliveOrigin::kFeedbackDialog:
      return out << "kFeedbackDialog";
    case ProfileKeepAliveOrigin::kWebAppUpdate:
      return out << "kWebAppUpdate";
    case ProfileKeepAliveOrigin::kGettingWebAppInfo:
      return out << "kGettingWebAppInfo";
    case ProfileKeepAliveOrigin::kCrxInstaller:
      return out << "kCrxInstaller";
    case ProfileKeepAliveOrigin::kProfilePickerView:
      return out << "kProfilePickerView";
    case ProfileKeepAliveOrigin::kDiceWebSigninInterceptionBubble:
      return out << "kDiceWebSigninInterceptionBubble";
    case ProfileKeepAliveOrigin::kHistoryMenuBridge:
      return out << "kHistoryMenuBridge";
    case ProfileKeepAliveOrigin::kProfileCreationSamlFlow:
      return out << "kProfileCreationSamlFlow";
    case ProfileKeepAliveOrigin::kProfileDeletionProcess:
      return out << "kProfileDeletionProcess";
    case ProfileKeepAliveOrigin::kProfileStatistics:
      return out << "kProfileStatistics";
    case ProfileKeepAliveOrigin::kIsolatedWebAppInstall:
      return out << "kIsolatedWebAppInstall";
    case ProfileKeepAliveOrigin::kIsolatedWebAppUpdate:
      return out << "kIsolatedWebAppUpdate";
    case ProfileKeepAliveOrigin::kWebAppUninstall:
      return out << "kWebAppUninstall";
    case ProfileKeepAliveOrigin::kOsIntegrationForceUnregistration:
      return out << "kOsIntegrationForceUnregistration";
    case ProfileKeepAliveOrigin::kRemoteDebugging:
      return out << "kRemoteDebugging";
    case ProfileKeepAliveOrigin::kHeadlessCommand:
      return out << "kHeadlessCommand";
  }
  NOTREACHED_IN_MIGRATION();
  return out << static_cast<int>(origin);
}
