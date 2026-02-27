// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_WEBUI_URL_CONSTANTS_H_
#define ASH_CONSTANTS_WEBUI_URL_CONSTANTS_H_

#include <string_view>

#include "base/component_export.h"

namespace ash {

// chrome: components (without schemes) and URLs (including schemes).
// e.g. kChromeUIFooHost = "foo" and kChromeUIFooURL = "chrome://foo/"
//
// This file should contain only for ash-chrome system related URLs.
// Web browser related URLs should be placed in
// chrome/common/webui_url_constants.h even if it is only for ChromeOS.
// NOTE: If you add a URL/host please check if it should be added to
// chrome::IsSystemWebUIHost() in chrome/common/webui_url_constants.h
//
// Please sort in lexicographical order.
inline constexpr char kChromeUIAccountManagerErrorHost[] =
    "account-manager-error";
inline constexpr char kChromeUIAccountManagerErrorURL[] =
    "chrome://account-manager-error";
inline constexpr char kChromeUIAccountMigrationWelcomeHost[] =
    "account-migration-welcome";
inline constexpr char kChromeUIAccountMigrationWelcomeURL[] =
    "chrome://account-migration-welcome";
inline constexpr char kChromeUIAddSupervisionHost[] = "add-supervision";
inline constexpr char kChromeUIAddSupervisionURL[] =
    "chrome://add-supervision/";
inline constexpr char kChromeUIAppDisabledHost[] = "app-disabled";
inline constexpr char kChromeUIAppDisabledURL[] = "chrome://app-disabled";
inline constexpr char kChromeUIAppInstallDialogHost[] = "app-install-dialog";
inline constexpr char kChromeUIAppInstallDialogURL[] =
    "chrome://app-install-dialog/";
inline constexpr char kChromeUIArcOverviewTracingHost[] =
    "arc-overview-tracing";
inline constexpr char kChromeUIArcPowerControlHost[] = "arc-power-control";
inline constexpr char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
inline constexpr char kChromeUIBluetoothPairingURL[] =
    "chrome://bluetooth-pairing/";
inline constexpr char kChromeUIBorealisCreditsHost[] = "borealis-credits";
inline constexpr char kChromeUIBorealisInstallerHost[] = "borealis-installer";
inline constexpr char kChromeUIBorealisInstallerURL[] =
    "chrome://borealis-installer";

// The host and URL for the Borealis MOTD Dialog
inline constexpr char kChromeUIBorealisMOTDHost[] = "borealis-motd";
inline constexpr char kChromeUIBorealisMOTDURL[] = "chrome://borealis-motd";

inline constexpr char kChromeUIChromeOSAssetHost[] = "chromeos-asset";
inline constexpr char kChromeUICloudUploadHost[] = "cloud-upload";
inline constexpr char kChromeUICloudUploadURL[] = "chrome://cloud-upload/";
inline constexpr char kChromeUIConfirmPasswordChangeHost[] =
    "confirm-password-change";
inline constexpr char kChromeUIConfirmPasswordChangeURL[] =
    "chrome://confirm-password-change";
inline constexpr char kChromeUICrostiniCreditsHost[] = "crostini-credits";
inline constexpr char16_t kChromeUICrostiniCreditsURL16[] =
    u"chrome://crostini-credits/";
inline constexpr char kChromeUICrostiniInstallerHost[] = "crostini-installer";
inline constexpr char kChromeUICrostiniInstallerURL[] =
    "chrome://crostini-installer";
inline constexpr char kChromeUICryptohomeHost[] = "cryptohome";
inline constexpr char kChromeUIDeviceEmulatorHost[] = "device-emulator";
inline constexpr char kChromeUIDlpInternalsHost[] = "dlp-internals";
inline constexpr char kChromeUIDriveInternalsHost[] = "drive-internals";
inline constexpr char kChromeUIEmojiPickerHost[] = "emoji-picker";
inline constexpr char kChromeUIEmojiPickerURL[] = "chrome://emoji-picker/";
inline constexpr char kChromeUIEnterpriseReportingHost[] =
    "enterprise-reporting";
inline constexpr char kChromeUIExtendedUpdatesDialogHost[] =
    "extended-updates-dialog";
inline constexpr char kChromeUIExtendedUpdatesDialogURL[] =
    "chrome://extended-updates-dialog";
inline constexpr char kChromeUIFloatingWorkspaceDialogHost[] =
    "floating-workspace";
inline constexpr char kChromeUIFloatingWorkspaceDialogURL[] =
    "chrome://floating-workspace";

// The host and URL for the Focus Mode media player.
inline constexpr char kChromeUIFocusModeMediaHost[] = "focus-mode-media";
inline constexpr char kChromeUIFocusModeMediaURL[] =
    "chrome://focus-mode-media";
inline constexpr char kChromeUIFocusModePlayerHost[] = "focus-mode-player";
inline constexpr char kChromeUIFocusModePlayerURL[] =
    "chrome-untrusted://focus-mode-player/";

inline constexpr char kChromeUIHealthdInternalsHost[] = "healthd-internals";
inline constexpr char kChromeUIInternetConfigDialogHost[] =
    "internet-config-dialog";
inline constexpr char kChromeUIInternetConfigDialogURL[] =
    "chrome://internet-config-dialog/";
inline constexpr char kChromeUIInternetDetailDialogHost[] =
    "internet-detail-dialog";
inline constexpr char kChromeUIInternetDetailDialogURL[] =
    "chrome://internet-detail-dialog/";
inline constexpr char kChromeUIKerberosInBrowserHost[] = "kerberos-in-browser";
inline constexpr char kChromeUIKerberosInBrowserURL[] =
    "chrome://kerberos-in-browser";
inline constexpr char kChromeUILauncherInternalsHost[] = "launcher-internals";
inline constexpr char kChromeUILocalFilesMigrationHost[] =
    "local-files-migration";
inline constexpr char kChromeUILocalFilesMigrationURL[] =
    "chrome://local-files-migration/";
inline constexpr char kChromeUILockScreenNetworkHost[] = "lock-network";
inline constexpr char kChromeUILockScreenNetworkURL[] = "chrome://lock-network";
inline constexpr char kChromeUILockScreenStartReauthHost[] = "lock-reauth";
inline constexpr char kChromeUILockScreenStartReauthURL[] =
    "chrome://lock-reauth";
inline constexpr char kChromeUIManageMirrorSyncHost[] = "manage-mirrorsync";
inline constexpr char kChromeUIManageMirrorSyncURL[] =
    "chrome://manage-mirrorsync";
inline constexpr char kChromeUIMobileSetupHost[] = "mobilesetup";
inline constexpr char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
inline constexpr char kChromeUIMultiDeviceInternalsHost[] =
    "multidevice-internals";
inline constexpr char kChromeUIMultiDeviceSetupHost[] = "multidevice-setup";
inline constexpr char kChromeUIMultiDeviceSetupURL[] =
    "chrome://multidevice-setup";
inline constexpr char kChromeUINetworkHost[] = "network";
inline constexpr char kChromeUINotificationTesterHost[] = "notification-tester";
inline constexpr char kChromeUIOfficeFallbackHost[] = "office-fallback";
inline constexpr char kChromeUIOfficeFallbackURL[] =
    "chrome://office-fallback/";
inline constexpr char kChromeUIOobeHost[] = "oobe";
inline constexpr char kChromeUIOobeURL[] = "chrome://oobe/";
inline constexpr char kChromeUIOSCreditsHost[] = "os-credits";
inline constexpr char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
inline constexpr char16_t kChromeUIOSCreditsURL16[] = u"chrome://os-credits/";

// Chrome OS tablet gestures education help link for Chrome.
// TODO(carpenterr): Have a solution for plink mapping in Help App.
// The magic numbers in this url are the topic and article ids currently
// required to navigate directly to a help article in the Help App.
inline constexpr char kChromeUIOSGestureEducationHelpURL[] =
    "chrome://help-app/help/sub/3399710/id/9739838";

inline constexpr char kChromeUIOSSettingsHost[] = "os-settings";
inline constexpr char kChromeUIOSSettingsURL[] = "chrome://os-settings/";
inline constexpr char kChromeUIParentAccessHost[] = "parent-access";
inline constexpr char kChromeUIParentAccessURL[] = "chrome://parent-access/";
inline constexpr char kChromeUIPasswordChangeHost[] = "password-change";
inline constexpr char kChromeUIPasswordChangeURL[] = "chrome://password-change";
inline constexpr char kChromeUIPowerHost[] = "power";
inline constexpr char kChromeUIRemoteManagementCurtainHost[] =
    "security-curtain";
inline constexpr char kChromeUISanitizeAppHost[] = "sanitize";
inline constexpr char kChromeUISanitizeAppURL[] = "chrome://sanitize";
inline constexpr char kChromeUISensorInfoHost[] = "sensor-info";
inline constexpr char kChromeUISetTimeHost[] = "set-time";
inline constexpr char kChromeUISetTimeURL[] = "chrome://set-time/";
inline constexpr char kChromeUISlowHost[] = "slow";
inline constexpr char kChromeUISlowTraceHost[] = "slow_trace";
inline constexpr char kChromeUISlowURL[] = "chrome://slow/";
inline constexpr char kChromeUISmbCredentialsHost[] = "smb-credentials-dialog";
inline constexpr char kChromeUISmbCredentialsURL[] =
    "chrome://smb-credentials-dialog/";
inline constexpr char kChromeUISmbShareHost[] = "smb-share-dialog";
inline constexpr char kChromeUISmbShareURL[] = "chrome://smb-share-dialog/";
inline constexpr char kChromeUISysInternalsHost[] = "sys-internals";
inline constexpr char kChromeUIUntrustedCroshHost[] = "crosh";
inline constexpr char kChromeUIUntrustedCroshURL[] =
    "chrome-untrusted://crosh/";
inline constexpr char kChromeUIUntrustedTerminalHost[] = "terminal";
inline constexpr char kChromeUIUntrustedTerminalURL[] =
    "chrome-untrusted://terminal/";
inline constexpr char kChromeUIUrgentPasswordExpiryNotificationHost[] =
    "urgent-password-expiry-notification";
inline constexpr char kChromeUIUrgentPasswordExpiryNotificationURL[] =
    "chrome://urgent-password-expiry-notification/";
inline constexpr char kChromeUIUserImageHost[] = "userimage";
inline constexpr char kChromeUIVmHost[] = "vm";

}  // namespace ash

#endif  // ASH_CONSTANTS_WEBUI_URL_CONSTANTS_H_
