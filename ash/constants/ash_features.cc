// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/system/sys_info.h"

namespace ash {
namespace features {
namespace {

// Controls whether Instant Tethering supports hosts which use the background
// advertisement model.
const base::Feature kInstantTetheringBackgroundAdvertisementSupport{
    "InstantTetheringBackgroundAdvertisementSupport",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace

// Enables redesign of account management flows.
// https://crbug.com/1132472
const base::Feature kAccountManagementFlowsV2{"AccountManagementFlowsV2",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether devices are updated before reboot after the first update.
const base::Feature kAllowRepeatedUpdates{"AllowRepeatedUpdates",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Shows settings for adjusting scroll acceleration/sensitivity for
// mouse/touchpad.
const base::Feature kAllowScrollSettings{"AllowScrollSettings",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable Ambient mode feature.
const base::Feature kAmbientModeFeature{"ChromeOSAmbientMode",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelAlbumEnabled{
    &kAmbientModeFeature, "CapturedOnPixelAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeFineArtAlbumEnabled{
    &kAmbientModeFeature, "FineArtAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotoAlbumEnabled{
    &kAmbientModeFeature, "FeaturedPhotoAlbumEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeEarthAndSpaceAlbumEnabled{
    &kAmbientModeFeature, "EarthAndSpaceAlbumEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeStreetArtAlbumEnabled{
    &kAmbientModeFeature, "StreetArtAlbumEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeDefaultFeedEnabled{
    &kAmbientModeFeature, "DefaultFeedEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModePersonalPhotosEnabled{
    &kAmbientModeFeature, "PersonalPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeFeaturedPhotosEnabled{
    &kAmbientModeFeature, "FeaturedPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeGeoPhotosEnabled{
    &kAmbientModeFeature, "GeoPhotosEnabled", true};

constexpr base::FeatureParam<bool> kAmbientModeCulturalInstitutePhotosEnabled{
    &kAmbientModeFeature, "CulturalInstitutePhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeRssPhotosEnabled{
    &kAmbientModeFeature, "RssPhotosEnabled", false};

constexpr base::FeatureParam<bool> kAmbientModeCapturedOnPixelPhotosEnabled{
    &kAmbientModeFeature, "CapturedOnPixelPhotosEnabled", false};

// Controls whether to enable Ambient mode album selection with photo previews.
const base::Feature kAmbientModePhotoPreviewFeature{
    "ChromeOSAmbientModePhotoPreview", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to allow Dev channel to use Prod server feature.
const base::Feature kAmbientModeDevUseProdFeature{
    "ChromeOSAmbientModeDevChannelUseProdServer",
    base::FEATURE_DISABLED_BY_DEFAULT};

// See https://crbug.com/1204551
const base::Feature kAppListBubble{"AppListBubble",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable ARC ADB sideloading support.
const base::Feature kArcAdbSideloadingFeature{
    "ArcAdbSideloading", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for ARC ADB sideloading for managed
// accounts and/or devices.
const base::Feature kArcManagedAdbSideloadingSupport{
    "ArcManagedAdbSideloadingSupport", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable support for View.onKeyPreIme() of ARC apps.
const base::Feature kArcPreImeKeyEventSupport{"ArcPreImeKeyEventSupport",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables auto screen-brightness adjustment when ambient light
// changes.
const base::Feature kAutoScreenBrightness{"AutoScreenBrightness",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable assistive autocorrect.
const base::Feature kAssistAutoCorrect{"AssistAutoCorrect",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive multi word suggestions.
const base::Feature kAssistMultiWord{"AssistMultiWord",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable assistive personal information.
const base::Feature kAssistPersonalInfo{"AssistPersonalInfo",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to suggest addresses in assistive personal information. This
// is only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoAddress{
    "AssistPersonalInfoAddress", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest emails in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoEmail{"AssistPersonalInfoEmail",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest names in assistive personal information. This is
// only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoName{"AssistPersonalInfoName",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to suggest phone numbers in assistive personal information.
// This is only effective when AssistPersonalInfo flag is enabled.
const base::Feature kAssistPersonalInfoPhoneNumber{
    "AssistPersonalInfoPhoneNumber", base::FEATURE_ENABLED_BY_DEFAULT};

// Displays the avatar toolbar button and the profile menu.
// https://crbug.com/1041472
extern const base::Feature kAvatarToolbarButton{
    "AvatarToolbarButton", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables more aggressive filtering out of Bluetooth devices with
// "appearances" that are less likely to be pairable or useful.
const base::Feature kBluetoothAggressiveAppearanceFilter{
    "BluetoothAggressiveAppearanceFilter", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Bluetooth WBS microphone be selected as default
// audio input option.
const base::Feature kBluetoothWbsDogfood{"BluetoothWbsDogfood",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the usage of fixed Bluetooth A2DP packet size to improve
// audio performance in noisy environment.
const base::Feature kBluetoothFixA2dpPacketSize{
    "BluetoothFixA2dpPacketSize", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables more filtering out of phones from the Bluetooth UI.
const base::Feature kBluetoothPhoneFilter{"BluetoothPhoneFilter",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the preference of using constant frame rate for camera
// when streaming.
const base::Feature kPreferConstantFrameRate{"PreferConstantFrameRate",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, will use the CDM in the Chrome OS daemon rather than loading the
// CDM using the library CDM interface.
const base::Feature kCdmFactoryDaemon{"CdmFactoryDaemon",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the value of |kCellularUseAttachApn| should have no effect and
// and the LTE attach APN configuration will not be sent to the modem. This
// flag exists because the |kCellularUseAttachApn| flag can be enabled
// by command-line arguments via board overlays which takes precedence over
// server-side field trial config, which may be needed to turn off the Attach
// APN feature.
const base::Feature kCellularForbidAttachApn{"CellularForbidAttachApn",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, send the LTE attach APN configuration to the modem.
const base::Feature kCellularUseAttachApn{"CellularUseAttachApn",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, use external euicc in Cellular Setup and Settings.
const base::Feature kCellularUseExternalEuicc{
    "CellularUseExternalEuicc", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, options page for each input method will be opened in ChromeOS
// settings. Otherwise it will be opened in a new web page in Chrome browser.
const base::Feature kImeOptionsInSettings{"ImeOptionsInSettings",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crosh System Web App. When enabled, crosh (Chrome OS
// Shell) will run as a tabbed System Web App rather than a normal browser tab.
const base::Feature kCroshSWA{"CroshSWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Crostini Disk Resizing.
const base::Feature kCrostiniDiskResizing{"CrostiniDiskResizing",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini using Buster container images.
const base::Feature kCrostiniUseBusterImage{"CrostiniUseBusterImage",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Crostini GPU support.
const base::Feature kCrostiniGpuSupport{"CrostiniGpuSupport",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Use DLC instead of component updater for managing the Termina image if set
// (and component updater instead of DLC if not).
const base::Feature kCrostiniUseDlc{"CrostiniUseDlc",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables using Cryptauth's GetDevicesActivityStatus API.
const base::Feature kCryptAuthV2DeviceActivityStatus{
    "CryptAuthV2DeviceActivityStatus", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables use of the connectivity status from Cryptauth's
// GetDevicesActivityStatus API to sort devices.
const base::Feature kCryptAuthV2DeviceActivityStatusUseConnectivity{
    "CryptAuthV2DeviceActivityStatusUseConnectivity",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Disable idle sockets closing on memory pressure for NetworkContexts that
// belong to Profiles. It only applies to Profiles because the goal is to
// improve perceived performance of web browsing within the Chrome OS user
// session by avoiding re-estabshing TLS connections that require client
// certificates.
const base::Feature kDisableIdleSocketsCloseOnMemoryPressure{
    "disable_idle_sockets_close_on_memory_pressure",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 DeviceSync flow. Regardless of this
// flag, v1 DeviceSync will continue to operate until it is disabled via the
// feature flag kDisableCryptAuthV1DeviceSync.
const base::Feature kCryptAuthV2DeviceSync{"CryptAuthV2DeviceSync",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the CryptAuth v2 Enrollment flow.
const base::Feature kCryptAuthV2Enrollment{"CryptAuthV2Enrollment",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDemoModeSWA{"DemoModeSWA",
                                 base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the Diagnostics app.
const base::Feature kDiagnosticsApp{"DiagnosticsApp",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Disables the CryptAuth v1 DeviceSync flow. Note: During the first phase
// of the v2 DeviceSync rollout, v1 and v2 DeviceSync run in parallel. This flag
// is needed to disable the v1 service during the second phase of the rollout.
// kCryptAuthV2DeviceSync should be enabled before this flag is flipped.
const base::Feature kDisableCryptAuthV1DeviceSync{
    "DisableCryptAuthV1DeviceSync", base::FEATURE_DISABLED_BY_DEFAULT};

// Disables "Office Editing for Docs, Sheets & Slides" component app so handlers
// won't be registered, making it possible to install another version for
// testing.
const base::Feature kDisableOfficeEditingComponentApp{
    "DisableOfficeEditingComponentApp", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, DriveFS will be used for Drive sync.
const base::Feature kDriveFs{"DriveFS", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables duplex native messaging between DriveFS and extensions.
const base::Feature kDriveFsBidirectionalNativeMessaging{
    "DriveFsBidirectionalNativeMessaging", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables DriveFS' experimental local files mirroring functionality.
const base::Feature kDriveFsMirroring{"DriveFsMirroring",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the System Web App (SWA) version of Eche.
const base::Feature kEcheSWA{"EcheSWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the naive resize for the Eche window.
const base::Feature kEcheSWAResizing{"EcheSWAResizing",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, emoji suggestion will be shown when user type "space".
const base::Feature kEmojiSuggestAddition{"EmojiSuggestAddition",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the DNS proxy service providing support split and secure DNS
// for Chrome OS.
const base::Feature kEnableDnsProxy{"EnableDnsProxy",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables setting the device hostname.
const base::Feature kEnableHostnameSetting{"EnableHostnameSetting",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the input device cards will be shown in the diagnostics app.
const base::Feature kEnableInputInDiagnosticsApp{
    "EnableInputInDiagnosticsApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables noise cancellation UI toggle.
const base::Feature kEnableInputNoiseCancellationUi{
    "EnableInputNoiseCancellationUi", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables LocalSearchService to be initialized.
const base::Feature kEnableLocalSearchService{"EnableLocalSearchService",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the networking cards will be shown in the diagnostics app.
const base::Feature kEnableNetworkingInDiagnosticsApp{
    "EnableNetworkingInDiagnosticsApp", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the OOBE ChromeVox hint dialog and announcement feature.
const base::Feature kEnableOobeChromeVoxHint{"EnableOobeChromeVoxHint",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables toggling Pciguard settings through Settings UI.
const base::Feature kEnablePciguardUi{"EnablePciguardUi",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables SAML re-authentication on the lock screen once the sign-in time
// limit expires.
const base::Feature kEnableSamlReauthenticationOnLockscreen{
    "EnableSamlReauthenticationOnLockScreen",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables Device End Of Lifetime warning notifications.
const base::Feature kEolWarningNotifications{"EolWarningNotifications",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable use of ordinal (unaccelerated) motion by Exo clients.
const base::Feature kExoOrdinalMotion{"ExoOrdinalMotion",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable pointer lock for Crostini windows.
const base::Feature kExoPointerLock{"ExoPointerLock",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable bubble showing when an application gains any UI lock.
const base::Feature kExoLockNotification{"ExoLockNotification",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Enables policy that controls feature to allow Family Link accounts on school
// owned devices.
const base::Feature kFamilyLinkOnSchoolDevice{"FamilyLinkOnSchoolDevice",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Whether to "hide" the legacy video player chrome app. Videos will instead be
// handled by the media app. See https://crbug.com/1158531.
const base::Feature kVideoPlayerAppHidden{"VideoPlayerAppHidden",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables partitioning of removable disks in file manager.
const base::Feature kFilesSinglePartitionFormat{
    "FilesSinglePartitionFormat", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the System Web App (SWA) version of file manager.
const base::Feature kFilesSWA{"FilesSWA", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable files app trash.
const base::Feature kFilesTrash{"FilesTrash",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Enables filters in Files app Recents view.
const base::Feature kFiltersInRecents{"FiltersInRecents",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables new ZIP archive handling in Files App.
// https://crbug.com/912236
const base::Feature kFilesZipMount{"FilesZipMount",
                                   base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kFilesZipPack{"FilesZipPack",
                                  base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kFilesZipUnpack{"FilesZipUnpack",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables handle of `closeView` message from Gaia. The message is
// supposed to end the flow.
const base::Feature kGaiaCloseViewMessage{"GaiaCloseViewMessage",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Gaia reauth endpoint with deleted user customization page.
const base::Feature kGaiaReauthEndpoint{"GaiaReauthEndpoint",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls gamepad vibration in Exo.
const base::Feature kGamepadVibration{"ExoGamepadVibration",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of Mojo by Chrome-process code to communicate with Power
// Manager. In order to use mojo, this feature must be turned on and a callsite
// must use PowerManagerMojoClient::Get().
const base::Feature kMojoDBusRelay{"MojoDBusRelay",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables pasting a few recently copied items in a menu when pressing search +
// v.
const base::Feature kClipboardHistory{"ClipboardHistory",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, the clipboard nudge shown prefs will be reset at the start of
// each new user session.
const base::Feature kClipboardHistoryNudgeSessionReset{
    "ClipboardHistoryNudgeSessionReset", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a blue new nudge will show on the context menu option for
// clipboard history.
const base::Feature kClipboardHistoryContextMenuNudge{
    "ClipboardHistoryContextMenuNudge", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the clipboard history shortcut will appear in screenshot
// notifications.
const base::Feature kClipboardHistoryScreenshotNudge{
    "ClipboardHistoryScreenshotNudge", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables copying an image to the system clipboard to support pasting onto
// different surfaces
const base::Feature kEnableFilesAppCopyImage{"EnableFilesAppCopyImage",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enable a D-Bus service for accessing gesture properties.
const base::Feature kGesturePropertiesDBusService{
    "GesturePropertiesDBusService", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables editing with handwriting gestures within the virtual keyboard.
const base::Feature kHandwritingGestureEditing{
    "HandwritingGestureEditing", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the Background Page in the help app.
const base::Feature kHelpAppBackgroundPage{"HelpAppBackgroundPage",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Discover Tab in the help app.
const base::Feature kHelpAppDiscoverTab{"HelpAppDiscoverTab",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables the Help App Discover tab notifications on non-stable
// Chrome OS channels. Used for testing.
const base::Feature kHelpAppDiscoverTabNotificationAllChannels{
    "HelpAppDiscoverTabNotificationAllChannels",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enable showing search results from the help app in the launcher.
const base::Feature kHelpAppLauncherSearch{"HelpAppLauncherSearch",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enable the search service integration in the Help app.
const base::Feature kHelpAppSearchServiceIntegration{
    "HelpAppSearchServiceIntegration", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable IME decoder via Mojo connection on Chrome OS.
const base::Feature kImeMojoDecoder{"ImeMojoDecoder",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable system emoji picker.
const base::Feature kImeSystemEmojiPicker{"SystemEmojiPicker",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable using the floating virtual keyboard as the default option
// on Chrome OS.
const base::Feature kVirtualKeyboardFloatingDefault{
    "VirtualKeyboardFloatingDefault", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Instant Tethering on Chrome OS.
const base::Feature kInstantTethering{"InstantTethering",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Kerberos Section in ChromeOS settings. When disabled, Kerberos
// settings will stay under People Section. https://crbug.com/983041
const base::Feature kKerberosSettingsSection{"KerberosSettingsSection",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Enables to use lacros-chrome as a primary web browser on Chrome OS.
// This works only when LacrosSupport below is enabled.
// NOTE: Use crosapi::browser_util::IsLacrosPrimary() instead of checking
// the feature directly. Similar to LacrosSupport, this may not be allowed
// depending on user types and/or policies.
const base::Feature kLacrosPrimary{"LacrosPrimary",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables "Linux and Chrome OS" support. Allows a Linux version of Chrome
// ("lacros-chrome") to run as a Wayland client with this instance of Chrome
// ("ash-chrome") acting as the Wayland server and window manager.
// NOTE: Use crosapi::browser_util::IsLacrosEnabled() instead of checking the
// feature directly. Lacros is not allowed for certain user types and can be
// disabled by policy. These restrictions will be lifted when Lacros development
// is complete.
const base::Feature kLacrosSupport{"LacrosSupport",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the second language settings update.
const base::Feature kLanguageSettingsUpdate2{"LanguageSettingsUpdate2",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables notification of when a microphone-using app is launched while the
// microphone is muted.
const base::Feature kMicMuteNotifications{"MicMuteNotifications",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable the requirement of a minimum chrome version on the
// device through the policy DeviceMinimumVersion. If the requirement is
// not met and the warning time in the policy has expired, the user is
// restricted from using the session.
const base::Feature kMinimumChromeVersion{"MinimumChromeVersion",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether new OOBE layout is shown or not.
const base::Feature kNewOobeLayout{"NewOobeLayout",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Whether image annotation is enabled in the ChromeOS media app.
const base::Feature kMediaAppAnnotation{"MediaAppAnnotation",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Whether EXIF metadata is displayed in the ChromeOS media app.
const base::Feature kMediaAppDisplayExif{"MediaAppDisplayExif",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Whether PDF files are opened by default in the ChromeOS media app.
const base::Feature kMediaAppHandlesPdf{"MediaAppHandlesPdf",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Whether to show the new Video controls UI in the ChromeOS media app.
const base::Feature kMediaAppVideoControls{"MediaAppVideoControls",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Enables support for multilingual assistive typing on Chrome OS.
const base::Feature kMultilingualTyping{"MultilingualTyping",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables support for specific enabled web apps to be treated as note-taking
// apps on Chrome OS.
const base::Feature kNoteTakingForEnabledWebApps{
    "NoteTakingForEnabledWebApps", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable on-device grammar check service.
const base::Feature kOnDeviceGrammarCheck{"OnDeviceGrammarCheck",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the feedback tool new UX on Chrome OS.
// This tool under development will be rolled out via Finch.
// Enabling this flag will use the new feedback tool instead of the current
// tool on CrOS.
const base::Feature kOsFeedback{"OsFeedback",
                                base::FEATURE_DISABLED_BY_DEFAULT};

// Whether the device supports on-device speech recognition.
const base::Feature kOnDeviceSpeechRecognition{
    "OnDeviceSpeechRecognition", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables a unique URL for each path in CrOS settings.
// This allows deep linking to individual settings, i.e. in settings search.
const base::Feature kOsSettingsDeepLinking{"OsSettingsDeepLinking",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

// Provides a UI for users to view information about their Android phone
// and perform phone-side actions within Chrome OS.
const base::Feature kPhoneHub{"PhoneHub", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables PIN setup in OOBE for Family Link users on all devices supporting low
// entropy credentials regardless the form factor.
const base::Feature kPinSetupForFamilyLink{"PinSetupForFamilyLink",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPinSetupForManagedUsers{"PinSetupForManagedUsers",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Hides shelf in immersive mode and allows esc hold to exit.
const base::Feature kPluginVmFullscreen{"PluginVmFullscreen",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the camera permissions should be shown in the Plugin
// VM app settings.
const base::Feature kPluginVmShowCameraPermissions{
    "PluginVmShowCameraPermissions", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether the microphone permissions should be shown in the Plugin
// VM app settings.
const base::Feature kPluginVmShowMicrophonePermissions{
    "PluginVmShowMicrophonePermissions", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to show printer statuses.
const base::Feature kPrinterStatus{"PrinterStatus",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to show printer statuses on the Print Preview destination
// dialog.
const base::Feature kPrinterStatusDialog{"PrinterStatusDialog",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

// Allows print servers to be selected when beyond a specified limit.
const base::Feature kPrintServerScaling{"PrintServerScaling",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable projector.
const base::Feature kProjector{"Projector", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable projector in status tray.
const base::Feature kProjectorFeaturePod{"ProjectorFeaturePod",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers.
const base::Feature kQuickAnswers{"QuickAnswers",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether dogfood version of quick answers.
const base::Feature kQuickAnswersDogfood{"QuickAnswersDogfood",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable quick answers text annotator.
const base::Feature kQuickAnswersTextAnnotator{
    "QuickAnswersTextAnnotator", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable quick answers translation.
const base::Feature kQuickAnswersTranslation{"QuickAnswersTranslation",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable quick answers translation using Cloud API.
const base::Feature kQuickAnswersTranslationCloudAPI{
    "QuickAnswersTranslationCloudAPI", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to trigger quick answers on editable text selection.
const base::Feature kQuickAnswersOnEditableText{
    "QuickAnswersOnEditableText", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to enable quick answers standalone settings section.
const base::Feature kQuickAnswersStandaloneSettings{
    "QuickAnswersStandaloneSettings", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the PIN auto submit feature is enabled.
const base::Feature kQuickUnlockPinAutosubmit{"QuickUnlockPinAutosubmit",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// TODO(crbug.com/1104164) - Remove this once most
// users have their preferences backfilled.
// Controls whether the PIN auto submit backfill operation should be performed.
const base::Feature kQuickUnlockPinAutosubmitBackfill{
    "QuickUnlockPinAutosubmitBackfill", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes notifications on Chrome OS.
const base::Feature kReleaseNotesNotification{"ReleaseNotesNotification",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables Release Notes notifications on non-stable Chrome OS
// channels. Used for testing.
const base::Feature kReleaseNotesNotificationAllChannels{
    "ReleaseNotesNotificationAllChannels", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables Release Notes suggestion chip on Chrome OS.
const base::Feature kReleaseNotesSuggestionChip{
    "ReleaseNotesSuggestionChip", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables showing a link to the Media app in the Scan app.
const base::Feature kScanAppMediaLink{"ScanAppMediaLink",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Enables or disables use of Searchable PDF file type in the Scan app.
const base::Feature kScanAppSearchablePdf{"ScanAppSearchablePdf",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables sticky settings in the Scan app.
const base::Feature kScanAppStickySettings{"ScanAppStickySettings",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables long kill timeout for session manager daemon. When
// enabled, session manager daemon waits for a longer time (e.g. 12s) for chrome
// to exit before sending SIGABRT. Otherwise, it uses the default time out
// (currently 3s).
const base::Feature kSessionManagerLongKillTimeout{
    "SessionManagerLongKillTimeout", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables the new shimless rma flow.
const base::Feature kShimlessRMAFlow{"ShimlessRMAFlow",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables a toggle to enable Bluetooth debug logs.
const base::Feature kShowBluetoothDebugLogToggle{
    "ShowBluetoothDebugLogToggle", base::FEATURE_ENABLED_BY_DEFAULT};

// Shows the Play Store icon in Demo Mode.
const base::Feature kShowPlayInDemoMode{"ShowPlayInDemoMode",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Uses experimental component version for smart dim.
const base::Feature kSmartDimExperimentalComponent{
    "SmartDimExperimentalComponent", base::FEATURE_DISABLED_BY_DEFAULT};

// This feature:
// - Creates a new "Sync your settings" section in Chrome OS settings
// - Moves app, wallpaper and Wi-Fi sync to OS settings
// - Provides a separate toggle for OS preferences, distinct from browser
//   preferences
// - Makes the OS ModelTypes run in sync transport mode, controlled by a single
//   pref for the entire OS sync feature
// - Updates the OOBE sync consent screen
//
// NOTE: The feature is rolling out via a client-side Finch trial, so the actual
// state will vary. See config in
// chrome/browser/ash/sync/split_settings_sync_field_trial.cc
const base::Feature kSplitSettingsSync{"SplitSettingsSync",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables using the system input engine for physical typing in
// languages based on latin script.
const base::Feature kSystemLatinPhysicalTyping{
    "SystemLatinPhysicalTyping", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables the Chrome OS system-proxy daemon, only for system services. This
// means that system services like tlsdate, update engine etc. can opt to be
// authenticated to a remote HTTP web proxy via system-proxy.
const base::Feature kSystemProxyForSystemServices{
    "SystemProxyForSystemServices", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables Chrome OS Telemetry Extension.
const base::Feature kTelemetryExtension{"TelemetryExtension",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the updated cellular activation UI; see go/cros-cellular-design.
const base::Feature kUpdatedCellularActivationUi{
    "UpdatedCellularActivationUi", base::FEATURE_ENABLED_BY_DEFAULT};

// Uses the same browser sync consent dialog as Windows/Mac/Linux. Allows the
// user to fully opt-out of browser sync, including marking the IdentityManager
// primary account as unconsented. Requires SplitSettingsSync.
// NOTE: Call UseBrowserSyncConsent() to test the flag, see implementation.
const base::Feature kUseBrowserSyncConsent{"UseBrowserSyncConsent",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging server as part of the Wallpaper App to verify
// additions/removals of wallpapers.
const base::Feature kUseWallpaperStagingUrl{"UseWallpaperStagingUrl",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Use the staging URL as part of the "Messages" feature under "Connected
// Devices" settings.
const base::Feature kUseMessagesStagingUrl{"UseMessagesStagingUrl",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Enables or disables user activity prediction for power management on
// Chrome OS.
// Defined here rather than in //chrome alongside other related features so that
// PowerPolicyController can check it.
const base::Feature kUserActivityPrediction{"UserActivityPrediction",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

// Remap search+click to right click instead of the legacy alt+click on
// Chrome OS.
const base::Feature kUseSearchClickForRightClick{
    "UseSearchClickForRightClick", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable bordered key for virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardBorderedKey{
    "VirtualKeyboardBorderedKey", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable or disable multipaste feature for virtual keyboard on Chrome OS.
const base::Feature kVirtualKeyboardMultipaste{
    "VirtualKeyboardMultipaste", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable the camera/mic indicators/notifications for VMs.
const base::Feature kVmCameraMicIndicatorsAndNotifications{
    "VmCameraMicIndicatorsAndNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable the chrome://vm page
const base::Feature kVmStatusPage{"VmStatusPage",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to allow enabling wake on WiFi features in shill.
const base::Feature kWakeOnWifiAllowed{"WakeOnWifiAllowed",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Enable new wallpaper experience in WebUI inside system settings.
const base::Feature kWallpaperWebUI{"WallpaperWebUI",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Generates WebAPKs representing installed PWAs and installs them inside ARC.
const base::Feature kWebApkGenerator{"WebApkGenerator",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable the syncing of deletes of Wi-Fi configurations.
// This only controls sending delete events to the Chrome Sync server.
const base::Feature kWifiSyncAllowDeletes{"WifiSyncAllowDeletes",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether to apply incoming Wi-Fi configuration delete events from
// the Chrome Sync server.
const base::Feature kWifiSyncApplyDeletes{"WifiSyncApplyDeletes",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether to enable syncing of Wi-Fi configurations between
// ChromeOS and a connected Android phone.
const base::Feature kWifiSyncAndroid{"WifiSyncAndroid",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enable or disable MOZC IME to use protobuf as interactive message format.
const base::Feature kImeMozcProto{"ImeMozcProto",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// Force enable recreating the LXD DB at LXD launch.
const base::Feature kCrostiniResetLxdDb{"CrostiniResetLxdDb",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the camera privacy switch toasts and notification should be
// displayed.
const base::Feature kCameraPrivacySwitchNotifications{
    "CameraPrivacySwitchNotifications", base::FEATURE_ENABLED_BY_DEFAULT};

////////////////////////////////////////////////////////////////////////////////

bool IsAccountManagementFlowsV2Enabled() {
  return base::FeatureList::IsEnabled(kAccountManagementFlowsV2);
}

bool IsAmbientModeEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeFeature);
}

bool IsAmbientModePhotoPreviewEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModePhotoPreviewFeature);
}

bool IsAmbientModeDevUseProdEnabled() {
  return base::FeatureList::IsEnabled(kAmbientModeDevUseProdFeature);
}

bool IsAppListBubbleEnabled() {
  return base::FeatureList::IsEnabled(kAppListBubble);
}

bool IsCellularActivationUiEnabled() {
  return base::FeatureList::IsEnabled(kUpdatedCellularActivationUi);
}

bool IsDemoModeSWAEnabled() {
  return base::FeatureList::IsEnabled(kDemoModeSWA);
}

bool IsDeepLinkingEnabled() {
  return base::FeatureList::IsEnabled(kOsSettingsDeepLinking);
}

bool IsDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kDiagnosticsApp);
}

bool IsEcheSWAEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWA);
}

bool IsEcheSWAResizingEnabled() {
  return base::FeatureList::IsEnabled(kEcheSWAResizing);
}

bool IsHostnameSettingEnabled() {
  return base::FeatureList::IsEnabled(kEnableHostnameSetting);
}

bool IsLocalSearchServiceEnabled() {
  return base::FeatureList::IsEnabled(kEnableLocalSearchService);
}

bool IsFamilyLinkOnSchoolDeviceEnabled() {
  return base::FeatureList::IsEnabled(kFamilyLinkOnSchoolDevice);
}

bool IsGaiaCloseViewMessageEnabled() {
  return base::FeatureList::IsEnabled(kGaiaCloseViewMessage);
}

bool IsGaiaReauthEndpointEnabled() {
  return base::FeatureList::IsEnabled(kGaiaReauthEndpoint);
}

bool IsInputInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableInputInDiagnosticsApp);
}

bool IsInputNoiseCancellationUiEnabled() {
  return base::FeatureList::IsEnabled(kEnableInputNoiseCancellationUi);
}

bool IsInstantTetheringBackgroundAdvertisingSupported() {
  return base::FeatureList::IsEnabled(
      kInstantTetheringBackgroundAdvertisementSupport);
}

bool IsKerberosSettingsSectionEnabled() {
  return base::FeatureList::IsEnabled(kKerberosSettingsSection);
}

bool IsMicMuteNotificationsEnabled() {
  return base::FeatureList::IsEnabled(kMicMuteNotifications);
}

bool IsMinimumChromeVersionEnabled() {
  return base::FeatureList::IsEnabled(kMinimumChromeVersion);
}

bool IsNetworkingInDiagnosticsAppEnabled() {
  return base::FeatureList::IsEnabled(kEnableNetworkingInDiagnosticsApp);
}

bool IsNewOobeLayoutEnabled() {
  return base::FeatureList::IsEnabled(kNewOobeLayout);
}

bool IsOobeChromeVoxHintEnabled() {
  return base::FeatureList::IsEnabled(kEnableOobeChromeVoxHint);
}

bool IsClipboardHistoryEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistory);
}

bool IsClipboardHistoryNudgeSessionResetEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryNudgeSessionReset);
}

bool IsClipboardHistoryContextMenuNudgeEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryContextMenuNudge);
}

bool IsClipboardHistoryScreenshotNudgeEnabled() {
  return base::FeatureList::IsEnabled(kClipboardHistoryScreenshotNudge);
}

bool IsPciguardUiEnabled() {
  return base::FeatureList::IsEnabled(kEnablePciguardUi);
}

bool IsPhoneHubEnabled() {
  return base::FeatureList::IsEnabled(kPhoneHub);
}

bool IsSamlReauthenticationOnLockscreenEnabled() {
  return base::FeatureList::IsEnabled(kEnableSamlReauthenticationOnLockscreen);
}

bool IsPinSetupForFamilyLinkEnabled() {
  return base::FeatureList::IsEnabled(kPinSetupForFamilyLink);
}

bool IsPinSetupForManagedUsersEnabled() {
  return base::FeatureList::IsEnabled(kPinSetupForManagedUsers);
}

bool IsPinAutosubmitFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmit);
}

bool IsPinAutosubmitBackfillFeatureEnabled() {
  return base::FeatureList::IsEnabled(kQuickUnlockPinAutosubmitBackfill);
}

bool IsProjectorEnabled() {
  return base::FeatureList::IsEnabled(kProjector);
}

bool IsProjectorFeaturePodEnabled() {
  return IsProjectorEnabled() &&
         base::FeatureList::IsEnabled(kProjectorFeaturePod);
}

bool IsQuickAnswersDogfood() {
  return base::FeatureList::IsEnabled(kQuickAnswersDogfood);
}

bool IsQuickAnswersEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswers);
}

bool IsQuickAnswersTranslationEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersTranslation);
}

bool IsQuickAnswersTranslationCloudAPIEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersTranslationCloudAPI);
}

bool IsQuickAnswersOnEditableTextEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersOnEditableText);
}

bool IsQuickAnswersStandaloneSettingsEnabled() {
  return base::FeatureList::IsEnabled(kQuickAnswersStandaloneSettings);
}

bool IsShimlessRMAFlowEnabled() {
  return base::FeatureList::IsEnabled(kShimlessRMAFlow);
}

bool IsSplitSettingsSyncEnabled() {
  return base::FeatureList::IsEnabled(kSplitSettingsSync);
}

bool IsSystemLatinPhysicalTypingEnabled() {
  return base::FeatureList::IsEnabled(kImeMojoDecoder) &&
         base::FeatureList::IsEnabled(kSystemLatinPhysicalTyping);
}

bool IsWallpaperWebUIEnabled() {
  return base::FeatureList::IsEnabled(kWallpaperWebUI);
}

bool IsWifiSyncAndroidEnabled() {
  return base::FeatureList::IsEnabled(kWifiSyncAndroid);
}

bool ShouldShowPlayStoreInDemoMode() {
  return base::FeatureList::IsEnabled(kShowPlayInDemoMode);
}

bool ShouldUseBrowserSyncConsent() {
  // UseBrowserSyncConsent requires SplitSettingsSync.
  return base::FeatureList::IsEnabled(kSplitSettingsSync) &&
         base::FeatureList::IsEnabled(kUseBrowserSyncConsent);
}

bool ShouldUseQuickAnswersTextAnnotator() {
  // The text classifier is only available on ChromeOS.
  return base::FeatureList::IsEnabled(kQuickAnswersTextAnnotator) &&
         base::SysInfo::IsRunningOnChromeOS();
}

bool ShouldUseV1DeviceSync() {
  return !ShouldUseV2DeviceSync() ||
         !base::FeatureList::IsEnabled(kDisableCryptAuthV1DeviceSync);
}

bool ShouldUseV2DeviceSync() {
  return base::FeatureList::IsEnabled(kCryptAuthV2Enrollment) &&
         base::FeatureList::IsEnabled(kCryptAuthV2DeviceSync);
}

bool ShouldUseAttachApn() {
  // See comment on |kCellularForbidAttachApn| for details.
  return !base::FeatureList::IsEnabled(kCellularForbidAttachApn) &&
         base::FeatureList::IsEnabled(kCellularUseAttachApn);
}

}  // namespace features
}  // namespace ash
