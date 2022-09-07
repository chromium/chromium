// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_NAMES_H_
#define ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_NAMES_H_

#include "base/component_export.h"

namespace ash {

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kCrosSettingsPrefix[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kAccountsPrefAllowGuest[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefAllowNewUser[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefShowUserNamesOnSignIn[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kAccountsPrefUsers[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefEphemeralUsersEnabled[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccounts[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyId[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyType[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyKioskAppId[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyKioskAppUpdateURL[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskPackage[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskClass[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskAction[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyArcKioskDisplayName[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyWebKioskUrl[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyWebKioskTitle[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountsKeyWebKioskIconUrl[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginId[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginDelay[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefDeviceLocalAccountPromptForNetworkWhenOffline[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefTransferSAMLCookies[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefLoginScreenDomainAutoComplete[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAccountsPrefFamilyLinkAccountsAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kSignedDataRoamingEnabled[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kUpdateDisabled[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kTargetVersionPrefix[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAllowedConnectionTypesForUpdate[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kSystemTimezonePolicy[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kSystemTimezone[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kSystemUse24HourClock[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kDeviceOwner[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kStatsReportingPref[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReleaseChannel[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReleaseChannelDelegated[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReleaseLtsTag[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceChannelDowngradeBehavior[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kEnableDeviceGranularReporting[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceVersionInfo[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceActivityTimes[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceAudioStatus[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceBoardStatus[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceBootMode[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceCpuInfo[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceTimezoneInfo[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceMemoryInfo[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceBacklightInfo[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceLocation[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceNetworkConfiguration[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceNetworkStatus[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDevicePeripherals[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDevicePowerStatus[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceStorageStatus[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceUsers[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceSecurityStatus[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceSessionStatus[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceGraphicsStatus[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceCrashReportInfo[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportOsUpdateStatus[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportRunningKioskApp[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportUploadFrequency[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceAppInfo[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceBluetoothInfo[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceFanInfo[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceVpdInfo[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDeviceSystemInfo[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kReportDevicePrintJobs[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceLoginLogout[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportCRDSessions[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceNetworkTelemetryCollectionRateMs[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceNetworkTelemetryEventCheckingRateMs[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceAudioStatusCheckingRateMs[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kReportDeviceSignalStrengthEventDrivenTelemetry[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kHeartbeatEnabled[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kHeartbeatFrequency[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kSystemLogUploadEnabled[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kPolicyMissingMitigationMode[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAllowRedeemChromeOsRegistrationOffers[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kFeatureFlags[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kKioskAppSettingsPrefix[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const int kKioskAppSettingsPrefixLength;
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kKioskApps[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kKioskAutoLaunch[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kKioskDisableBailoutShortcut[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kVariationsRestrictParameter[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceAttestationEnabled[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kAttestationForContentProtectionEnabled[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kServiceAccountIdentity[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kDeviceDisabled[];
COMPONENT_EXPORT(ASH_SETTINGS) extern const char kDeviceDisabledMessage[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kRebootOnShutdown[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kExtensionCacheSize[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDisplayResolution[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalWidth[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalHeight[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalScale[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDisplayResolutionKeyExternalUseNative[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDisplayResolutionKeyInternalScale[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDisplayResolutionKeyRecommended[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kDisplayRotationDefault[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kKioskCRXManifestUpdateURLIgnored[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kLoginAuthenticationBehavior[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kAllowBluetooth[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kDeviceWiFiAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceQuirksDownloadEnabled[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kLoginVideoCaptureAllowedUrls[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceLoginScreenLocales[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceLoginScreenInputMethods[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceLoginScreenSystemInfoEnforced[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceShowNumericKeyboardForPassword[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kPerUserTimezoneEnabled[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kFineGrainedTimeZoneResolveEnabled[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kDeviceOffHours[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDevicePrintersAccessMode[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDevicePrintersBlocklist[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDevicePrintersAllowlist[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kTPMFirmwareUpdateSettings[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceMinimumVersion[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceMinimumVersionAueMessage[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kCastReceiverName[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kUnaffiliatedArcAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kDeviceHostnameTemplate[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceHostnameUserConfigurable[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kVirtualMachinesAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceAutoUpdateTimeRestrictions[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceUnaffiliatedCrostiniAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kPluginVmAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kBorealisAllowedForDevice[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceRebootOnUserSignout[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceWilcoDtcAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceDockMacAddressSource[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceScheduledUpdateCheck[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceSecondFactorAuthenticationMode[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDevicePowerwashAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceWebBasedAttestationAllowedUrls[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kSystemProxySettings[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kSystemProxySettingsKeyEnabled[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kSystemProxySettingsKeySystemServicesUsername[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kSystemProxySettingsKeySystemServicesPassword[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kSystemProxySettingsKeyAuthSchemes[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceCrostiniArcAdbSideloadingAllowed[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceShowLowDiskSpaceNotification[];

COMPONENT_EXPORT(ASH_SETTINGS) extern const char kUsbDetachableAllowlist[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kUsbDetachableAllowlistKeyVid[];
COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kUsbDetachableAllowlistKeyPid[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDevicePeripheralDataAccessEnabled[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceAllowedBluetoothServices[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceScheduledReboot[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceRestrictedManagedGuestSessionEnabled[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kRevenEnableDeviceHWDataUsage[];

COMPONENT_EXPORT(ASH_SETTINGS)
extern const char kDeviceEncryptedReportingPipelineEnabled[];

}  // namespace ash

namespace chromeos {
using ::ash::kAccountsPrefAllowNewUser;
using ::ash::kAccountsPrefDeviceLocalAccountAutoLoginBailoutEnabled;
using ::ash::kAccountsPrefFamilyLinkAccountsAllowed;
using ::ash::kAccountsPrefLoginScreenDomainAutoComplete;
using ::ash::kDeviceCrostiniArcAdbSideloadingAllowed;
using ::ash::kDeviceOwner;
using ::ash::kDevicePeripheralDataAccessEnabled;
using ::ash::kDeviceSecondFactorAuthenticationMode;
using ::ash::kDeviceWebBasedAttestationAllowedUrls;
using ::ash::kFineGrainedTimeZoneResolveEnabled;
using ::ash::kLoginAuthenticationBehavior;
using ::ash::kSystemTimezone;
using ::ash::kSystemTimezonePolicy;
using ::ash::kTPMFirmwareUpdateSettings;
}  // namespace chromeos

#endif  // ASH_COMPONENTS_SETTINGS_CROS_SETTINGS_NAMES_H_
