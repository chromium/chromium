// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_URL_CONSTANTS_H_
#define ASH_CONSTANTS_URL_CONSTANTS_H_

// Contains constants for known URLs for OS system parts of chromeos-chrome.
//
// - Keep the constants sorted by name within its section.

namespace ash::external_urls {

// Accessibility help link.
inline constexpr char kAccessibilityHelpURL[] =
    "https://support.google.com/chromebook/topic/6323347";

// Help center URL for Chrome OS Account Manager.
inline constexpr char kAccountManagerLearnMoreURL[] =
    "https://support.google.com/chromebook?p=google_accounts";

// The URL for the "learn more" link for Google Play Store (ARC) settings.
inline constexpr char kAndroidAppsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=playapps";

// The URL for the "Learn more" link for Android Messages.
inline constexpr char kAndroidMessagesLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multi_device_messages";

// "Learn more" URL for APN settings.
inline constexpr char16_t kApnSettingsLearnMoreUrl[] =
    u"https://support.google.com/chromebook?p=apn";

// "Learn more" URL for App Parental Controls.
// char16_t is used here because this constant may be used to set the src
// attribute of iframe elements.
inline constexpr char16_t kAppParentalControlsLearnMoreUrl[] =
    u"https://support.google.com/chromebook?p=local_app_controls";

// The URL for the "Learn more" link in the External storage preferences
// settings.
inline constexpr char16_t kArcExternalStorageLearnMoreURL[] =
    u"https://support.google.com/chromebook?p=open_files";

// The URL path to online ARC++ terms of service.
inline constexpr char kArcTosOnlineURLPath[] =
    "https://play.google/play-terms/embedded/";

// The URL for Auto Update Policy.
inline constexpr char16_t kAutoUpdatePolicyURL[] =
    u"https://support.google.com/chrome/a?p=auto-update-policy";

// The URL for the "Learn more" link during Bluetooth pairing.
// TODO(crbug.com/1010321): Remove 'm100' prefix from link once Bluetooth Revamp
// has shipped.
inline constexpr char16_t kBluetoothPairingLearnMoreUrl[] =
    u"https://support.google.com/chromebook?p=bluetooth_revamp_m100";

// The URL for the "learn more" link for cellular carrier lock.
// TODO(b/293463820): Replace the link with carrier lock link once ready.
inline constexpr char kCellularCarrierLockLearnMoreURL[] =
    "https://support.google.com/chromebook";

// The URL for the "learn more" link for Chromebook hotspot.
inline constexpr char kChromebookHotspotLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_hotspot";

// The URL for the Chromebook Perks page for YouTube.
inline constexpr char kChromebookPerksYouTubePage[] =
    "https://www.google.com/chromebook/perks/?id=youtube.2020";

// The URL for the "Learn more" link for scrolling acceleration on ChromeOS.
// TODO(zhangwenyu): Update link once confirmed.
inline constexpr char kControlledScrollingHelpURL[] =
    "https://support.google.com/chromebook?p=simple_scrolling";

// Help center URL for ChromeOS Battery Saver.
inline constexpr char kCrosBatterySaverLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=battery_saver";

// The URL path to Online Chrome and Chrome OS terms of service.
inline constexpr char kCrosEulaOnlineURLPath[] =
    "https://www.google.com/intl/%s/chrome/terms/";

inline constexpr char kCupsPrintLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_printing";

inline constexpr char kCupsPrintPPDLearnMoreURL[] =
    "https://support.google.com/chromebook?p=printing_advancedconfigurations";

// The URL for the learn more link about extended automatic updates for
// ChromeOS devices.
inline constexpr char16_t kDeviceExtendedUpdatesLearnMoreURL[] =
    u"https://www.google.com/chromebook/autoupdates-opt-in/";

// The URL for the "Learn more" link the the Easy Unlock settings.
inline constexpr char kEasyUnlockLearnMoreUrl[] =
    "https://support.google.com/chromebook?p=smart_lock";

// The URL for EOL notification
inline constexpr char16_t kEolNotificationURL[] =
    u"https://www.google.com/chromebook/older/";

// The URL for the "Learn more" page for the Face control feature on Chrome OS.
inline constexpr char kFaceGazeLearnMoreURL[] =
    "https://support.google.com/chromebook?p=face_control";

// The URL for the help center article about fingerprint on Chrome OS devices.
inline constexpr char kFingerprintLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_fingerprint";

// The URL for the "Learn more" page when the user tries to clean up their
// Google Drive offline storage in the OS settings page.
inline constexpr char kGoogleDriveCleanUpStorageLearnMoreURL[] =
    "https://support.google.com/chromebook?p=cleanup_offline_files";

inline constexpr char kGoogleDriveOfflineLearnMoreURL[] =
    "https://support.google.com/chromebook?p=my_drive_cbx";

// The URL path to Online Google EULA.
inline constexpr char kGoogleEulaOnlineURLPath[] =
    "https://policies.google.com/terms/embedded?hl=%s";

// The URL for providing more information about Google nameservers.
inline constexpr char kGoogleNameserversLearnMoreURL[] =
    "https://developers.google.com/speed/public-dns";

// URL for the Google Privacy Policy.
inline constexpr char kGooglePrivacyPolicyUrl[] =
    "https://policies.google.com/privacy";

// The URL for the "Learn more" link for touchpad haptic feedback on Chrome OS.
inline constexpr char kHapticFeedbackHelpURL[] =
    "https://support.google.com/chromebook?p=haptic_feedback_m100";

// "Learn more" URL for Help Me Read and Help Me Write feature on ChromeOS.
inline constexpr char kHelpMeReadWriteLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=settings_help_me_read_write";

// The URL for "Learn more" page for Isolated Web Apps.
// TODO(crbug.com/40281470): Update this URL with proper user-facing explainer.
inline constexpr char16_t kIsolatedWebAppsLearnMoreUrl[] =
    u"https://github.com/WICG/isolated-web-apps/blob/main/README.md";

// The URL for the "learn more" link for Instant Tethering.
inline constexpr char kInstantTetheringLearnMoreURL[] =
    "https://support.google.com/chromebook?p=instant_tethering";

// The URL for the "Learn more" link for Kerberos accounts.
inline constexpr char kKerberosAccountsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=kerberos_accounts";

// The URL for the "Learn more" link in language settings regarding language
// packs.
inline constexpr char16_t kLanguagePacksLearnMoreURL[] =
    u"https://support.google.com/chromebook?p=language_packs";

// The URL for the "Learn more" link in the language settings.
inline constexpr char16_t kLanguageSettingsLearnMoreUrl[] =
    u"https://support.google.com/chromebook?p=order_languages";

// The URL for the Learn More page about Linux for Chromebooks.
inline constexpr char kLinuxAppsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_linuxapps";

// The URL for additional help that is given when Linux export/import fails.
inline constexpr char kLinuxExportImportHelpURL[] =
    "https://support.google.com/chromebook?p=linux_backup_restore";

// "Learn more" URL for Lobster feature on ChromeOS.
inline constexpr char kLobsterLearnMoreURL[] =
    "https://support.google.com/chromebook?p=dugong2";

// The URL for the "Learn more" link for natural scrolling on ChromeOS.
inline constexpr char kNaturalScrollHelpURL[] =
    "https://support.google.com/chromebook?p=simple_scrolling";

// The URL for the "Learn more" link in the connected devices.
inline constexpr char kMultiDeviceLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multi_device";

// Palette help link for Chrome.
inline constexpr char kPaletteHelpURL[] =
    "https://support.google.com/chromebook?p=stylus_help";

// The URL for the "Learn more" link in the peripheral data access protection
// settings.
inline constexpr char kPeripheralDataAccessHelpURL[] =
    "https://support.google.com/chromebook?p=connect_thblt_usb4_accy";

// "Learn more" URL for the phone hub notifications and apps access setup.
// TODO (b/184137843): Use real link to phone hub notifications and apps access.
inline constexpr char kPhoneHubPermissionLearnMoreURL[] =
    "https://support.google.com/chromebook?p=multidevice";

// The URL for the "Learn more" page for the Location Accuracy setting under the
// privacy hub location subpage.
// TODO(crbug.com/485470549): migrate with
// ash/webui/personalization_app/personalization_app_url_constants.cc
inline constexpr char16_t kPrivacyHubGeolocationAccuracyLearnMoreURL[] =
    u"https://support.google.com/android/?p=location_accuracy";

// The URL for the "Learn more" page for the geolocation area in the privacy
// hub page.
inline constexpr char kPrivacyHubGeolocationLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=manage_your_location";

// The URL for the help center article about local data recovery on Chrome OS
// devices.
inline constexpr char kRecoveryLearnMoreURL[] =
    "https://support.google.com/chromebook?p=local_data_recovery";

inline constexpr char kSafetyPixelbookURL[] = "https://g.co/Pixelbook/legal";
inline constexpr char kSafetyPixelSlateURL[] = "https://g.co/PixelSlate/legal";

// "Learn more" URL for Scanner feature on ChromeOS.
inline constexpr char kScannerLearnMoreUrl[] =
    "https://support.google.com/chromebook?p=dugong3";

// The URL for the "Learn more" link for Enhanced network voices in Chrome OS
// settings for Select-to-speak.
inline constexpr char kSelectToSpeakLearnMoreURL[] =
    "https://support.google.com/chromebook?p=select_to_speak";

// The URL for the "Learn more" page for screen privacy protections.
inline constexpr char kSmartPrivacySettingsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=screen_privacy_m100";

// The URL for the "Learn more" page for the network file shares settings page.
inline constexpr char kSmbSharesLearnMoreURL[] =
    "https://support.google.com/chromebook?p=network_file_shares";

// The URL for the "Learn more" page for Speak-on-mute Detection in the privacy
// hub page.
inline constexpr char kSpeakOnMuteDetectionLearnMoreURL[] =
    "https://support.google.com/chromebook?p=mic-mute";

// The URL for the "Learn more" page for Suggested Content in the privacy page.
inline constexpr char kSuggestedContentLearnMoreURL[] =
    "https://support.google.com/chromebook?p=explorecontent";

// The URL to a support article with more information about gestures available
// in tablet mode on Chrome OS (gesture to go to home screen, overview, or to go
// back). Used as a "Learn more" link URL for the accessibility option to shelf
// navigation buttons in tablet mode (the buttons are hidden by default in
// favour of the gestures in question).
inline constexpr char kTabletModeGesturesLearnMoreURL[] =
    "https://support.google.com/chromebook?p=tablet_mode_gestures";

// The URL for the "Learn more" page for the time zone settings page.
inline constexpr char kTimeZoneSettingsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_timezone&hl=%s";

// The URL for the "learn more" link for TPM firmware update.
inline constexpr char kTPMFirmwareUpdateLearnMoreURL[] =
    "https://support.google.com/chromebook?p=tpm_update";

// The URL for the help center article about video chat enhanced features.
inline constexpr char kVcLearnMoreURL[] =
    "https://support.google.com/chromebook/answer/10264237"
    "#zippy=enhanced-features-available-on-chromebook-plus";

// The URL for the help center article about hidden Wi-Fi networks.
inline constexpr char kWifiHiddenNetworkURL[] =
    "https://support.google.com/chromebook?p=hidden_networks";

// The URL for the help center article about Passpoint.
inline constexpr char kWifiPasspointURL[] =
    "https://support.google.com/chromebook?p=wifi_passpoint";

// The URL for the help center article about Wi-Fi sync.
inline constexpr char kWifiSyncLearnMoreURL[] =
    "https://support.google.com/chromebook?p=wifisync";

// The URL for the YoutTube Music Premium signup page.
inline constexpr char kYoutubeMusicPremiumURL[] =
    "https://music.youtube.com/music_premium";

}  // namespace ash::external_urls

#endif  // ASH_CONSTANTS_URL_CONSTANTS_H_
