// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_

#include <optional>
#include <string>
#include <string_view>

#include "ash/public/cpp/ash_public_export.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/version_info/channel.h"
#include "url/gurl.h"

namespace ash {

// Handles method calls delegated back to chrome from ash.
class ASH_PUBLIC_EXPORT SystemTrayClient {
 public:
  virtual ~SystemTrayClient() {}

  // Shows general settings UI.
  virtual void ShowSettings(int64_t display_id) = 0;

  // Shows settings related to the user account.
  virtual void ShowAccountSettings() = 0;

  // Shows settings related to Bluetooth devices (e.g. to add a device).
  virtual void ShowBluetoothSettings() = 0;

  // Shows the detailed settings for the Bluetooth device with ID |device_id|.
  virtual void ShowBluetoothSettings(const std::string& device_id) = 0;

  // Show the Bluetooth pairing dialog. When provided, |device_address| is the
  // unique device address that the dialog should attempt to pair with and
  // should be in the form "XX:XX:XX:XX:XX:XX". When |device_address| is not
  // provided the dialog will show the device list instead.
  virtual void ShowBluetoothPairingDialog(
      std::optional<std::string_view> device_address) = 0;

  // Shows the settings related to date, timezone etc.
  virtual void ShowDateSettings() = 0;

  // Shows the dialog to set system time, date, and timezone.
  virtual void ShowSetTimeDialog() = 0;

  // Shows settings related to multiple displays.
  virtual void ShowDisplaySettings() = 0;

  // Shows settings related to multiple displays.
  virtual void ShowDarkModeSettings() = 0;

  // Shows settings related to storage.
  virtual void ShowStorageSettings() = 0;

  // Shows settings related to power.
  virtual void ShowPowerSettings() = 0;

  // Shows OS settings related to privacy and security.
  virtual void ShowPrivacyAndSecuritySettings() = 0;

  // Shows OS settings page for Privacy Hub.
  virtual void ShowPrivacyHubSettings() = 0;

  // Shows OS settings page for speak-on-mute detection setting in Privacy Hub.
  virtual void ShowSpeakOnMuteDetectionSettings() = 0;

  // Show OS smart privacy settings.
  virtual void ShowSmartPrivacySettings() = 0;

  // Shows the page that lets you disable performance tracing.
  virtual void ShowChromeSlow() = 0;

  // Shows settings related to input methods.
  virtual void ShowIMESettings() = 0;

  // Shows settings related to MultiDevice features.
  virtual void ShowConnectedDevicesSettings() = 0;

  // Shows settings related to tether network.
  virtual void ShowTetherNetworkSettings() = 0;

  // Shows settings related to Wi-Fi Sync v2.
  virtual void ShowWifiSyncSettings() = 0;

  // Shows the about chrome OS page and checks for updates after the page is
  // loaded.
  virtual void ShowAboutChromeOS() = 0;

  // Shows the about chrome OS additional details page.
  virtual void ShowAboutChromeOSDetails() = 0;

  // Shows accessibility help.
  virtual void ShowAccessibilityHelp() = 0;

  // Shows the settings related to accessibility.
  virtual void ShowAccessibilitySettings() = 0;

  // Shows the settings related to color correction.
  virtual void ShowColorCorrectionSettings() = 0;

  // Shows gesture education help.
  virtual void ShowGestureEducationHelp() = 0;

  // Shows the help center article for the stylus tool palette.
  virtual void ShowPaletteHelp() = 0;

  // Shows the settings related to the stylus tool palette.
  virtual void ShowPaletteSettings() = 0;

  // Shows information about enterprise enrolled devices.
  virtual void ShowEnterpriseInfo() = 0;

  // Shows UI to configure or activate the network specified by |network_id|,
  // which may include showing payment or captive portal UI when appropriate.
  virtual void ShowNetworkConfigure(const std::string& network_id) = 0;

  // Shows UI to create a new network connection. |type| is the ONC network type
  // (see onc_spec.md). TODO(stevenjb): Use NetworkType from onc.mojo (TBD).
  virtual void ShowNetworkCreate(const std::string& type) = 0;

  // Opens the cellular setup flow in OS Settings. |show_psim_flow| indicates
  // if we should navigate to the physical SIM setup flow or to the page that
  // allows the user to select which flow they wish to enter (pSIM or eSIM).
  virtual void ShowSettingsCellularSetup(bool show_psim_flow) = 0;

  // Opens Mobile data subpage.
  virtual void ShowMobileDataSubpage() = 0;

  // Opens SIM unlock dialog in OS Settings.
  virtual void ShowSettingsSimUnlock() = 0;

  // Opens the APN subpage for network with guid |network_id|.
  virtual void ShowApnSubpage(const std::string& network_id) = 0;

  // Shows the "add network" UI to create a third-party extension-backed VPN
  // connection (e.g. Cisco AnyConnect).
  virtual void ShowThirdPartyVpnCreate(const std::string& extension_id) = 0;

  // Launches Arc VPN provider.
  virtual void ShowArcVpnCreate(const std::string& app_id) = 0;

  // Shows settings related to networking. If |network_id| is empty, shows
  // general settings. Otherwise shows settings for the individual network.
  // On devices |network_id| is a GUID, but on Linux desktop and in tests it can
  // be any string.
  virtual void ShowNetworkSettings(const std::string& network_id) = 0;

  // Shows the Hotspot subpage.
  virtual void ShowHotspotSubpage() = 0;

  // Shows the MultiDevice setup flow dialog.
  virtual void ShowMultiDeviceSetup() = 0;

  // Shows the Firmware update app.
  virtual void ShowFirmwareUpdate() = 0;

  // Sets the UI locale to |locale_iso_code| and exit the session to take
  // effect.
  virtual void SetLocaleAndExit(const std::string& locale_iso_code) = 0;

  // Shows the access code casting dialog. |open_location| is the location
  // where the dialog was opened.
  virtual void ShowAccessCodeCastingDialog(
      AccessCodeCastDialogOpenLocation open_location) = 0;

  // Shows a calendar event. If an event is present then it's opened, otherwise
  // Google Calendar is opened to `date`. Open in the calendar PWA if
  // installed (and assign true to `opened_pwa`), in a new browser tab otherwise
  // (and assign false to |opened_pwa|).
  //
  // The calendar PWA requires the event URL to have a specific prefix,
  // so the URL actually opened may not be the same as the passed-in URL.  This
  // is guaranteed to be the case if no event URL was passed in.  The URL that's
  // actually opened is assigned to `finalized_event_url`.
  virtual void ShowCalendarEvent(const std::optional<GURL>& event_url,
                                 const base::Time& date,
                                 bool& opened_pwa,
                                 GURL& finalized_event_url) = 0;

  // Launches video conference url's.
  // Opens in the Google Meet PWA if installed and `video_conference_url` is a
  // Google Meet url, otherwise opens in the browser.
  virtual void ShowVideoConference(const GURL& video_conference_url) = 0;

  // Shown when the device is on a non-stable release track and the user clicks
  // the channel/version button from quick settings.
  virtual void ShowChannelInfoAdditionalDetails() = 0;

  // Shown when the device is on a non-stable release track and the user clicks
  // the "send feedback" button.
  virtual void ShowChannelInfoGiveFeedback() = 0;

  // Shows settings related to audio devices (input/output).
  virtual void ShowAudioSettings() = 0;

  // Shows a page with more info about auto update expiration for devices that
  // reached end of life.
  virtual void ShowEolInfoPage() = 0;

  // Records a UMA metric that the end of life notice was shown.
  virtual void RecordEolNoticeShown() = 0;

  // Returns 'true' if the user preference is set to allow users to submit
  // feedback, 'false' otherwise.
  virtual bool IsUserFeedbackEnabled() = 0;

  // Shows settings related to graphics tablets.
  virtual void ShowGraphicsTabletSettings() = 0;

  // Shows settings related to mice.
  virtual void ShowMouseSettings() = 0;

  // Shows settings related to touchpads.
  virtual void ShowTouchpadSettings() = 0;

  // Shows the remap keyboard keys settings subpage for the keyboard with
  // `device_id`.
  virtual void ShowRemapKeysSubpage(int device_id) = 0;

  // Shows a page about premium plans.
  virtual void ShowYouTubeMusicPremiumPage() = 0;
  virtual void ShowChromebookPerksYouTubePage() = 0;

  // Shows settings related to keyboards.
  virtual void ShowKeyboardSettings() = 0;

  // Shows settings related to pointing sticks.
  virtual void ShowPointingStickSettings() = 0;

  // Shows settings related to Quick Share (formerly Nearby Share).
  virtual void ShowNearbyShareSettings() = 0;

 protected:
  SystemTrayClient() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_
