// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_system_tray_client.h"

#include <string_view>

namespace ash {

TestSystemTrayClient::TestSystemTrayClient() = default;

TestSystemTrayClient::~TestSystemTrayClient() = default;

void TestSystemTrayClient::ShowSettings(int64_t display_id) {}

void TestSystemTrayClient::ShowAccountSettings() {
  show_account_settings_count_++;
}

void TestSystemTrayClient::ShowBluetoothSettings() {
  show_bluetooth_settings_count_++;
}

void TestSystemTrayClient::ShowBluetoothSettings(const std::string& device_id) {
  show_bluetooth_settings_count_++;
  last_bluetooth_settings_device_id_ = device_id;
}

void TestSystemTrayClient::ShowBluetoothPairingDialog(
    std::optional<std::string_view> device_address) {
  show_bluetooth_pairing_dialog_count_++;
}

void TestSystemTrayClient::ShowDateSettings() {}

void TestSystemTrayClient::ShowSetTimeDialog() {}

void TestSystemTrayClient::ShowDisplaySettings() {}

void TestSystemTrayClient::ShowDarkModeSettings() {}

void TestSystemTrayClient::ShowStorageSettings() {}

void TestSystemTrayClient::ShowPowerSettings() {}

void TestSystemTrayClient::ShowChromeSlow() {}

void TestSystemTrayClient::ShowIMESettings() {}

void TestSystemTrayClient::ShowConnectedDevicesSettings() {
  show_connected_devices_settings_count_++;
}

void TestSystemTrayClient::ShowTetherNetworkSettings() {}

void TestSystemTrayClient::ShowWifiSyncSettings() {
  show_wifi_sync_settings_count_++;
}

void TestSystemTrayClient::ShowAboutChromeOS() {
  show_about_chromeos_count_++;
}

void TestSystemTrayClient::ShowAboutChromeOSDetails() {}

void TestSystemTrayClient::ShowAccessibilityHelp() {}

void TestSystemTrayClient::ShowAccessibilitySettings() {}

void TestSystemTrayClient::ShowColorCorrectionSettings() {
  show_color_correction_settings_count_++;
}

void TestSystemTrayClient::ShowGestureEducationHelp() {}

void TestSystemTrayClient::ShowPaletteHelp() {}

void TestSystemTrayClient::ShowPaletteSettings() {}

void TestSystemTrayClient::ShowPrivacyAndSecuritySettings() {
  show_os_settings_privacy_and_security_count_++;
}

void TestSystemTrayClient::ShowPrivacyHubSettings() {
  show_os_settings_privacy_hub_count_++;
}

void TestSystemTrayClient::ShowSpeakOnMuteDetectionSettings() {
  show_speak_on_mute_detection_count_++;
}

void TestSystemTrayClient::ShowSmartPrivacySettings() {
  show_os_smart_privacy_settings_count_++;
}

void TestSystemTrayClient::ShowEnterpriseInfo() {}

void TestSystemTrayClient::ShowNetworkConfigure(const std::string& network_id) {
}

void TestSystemTrayClient::ShowNetworkCreate(const std::string& type) {
  show_network_create_count_++;
  last_network_type_ = type;
}

void TestSystemTrayClient::ShowSettingsCellularSetup(bool show_psim_flow) {}

void TestSystemTrayClient::ShowMobileDataSubpage() {
  show_mobile_data_subpage_count_++;
}

void TestSystemTrayClient::ShowSettingsSimUnlock() {
  ++show_sim_unlock_settings_count_;
}

void TestSystemTrayClient::ShowApnSubpage(const std::string& network_id) {
  ++show_apn_subpage_count_;
  last_apn_subpage_network_id_ = network_id;
}

void TestSystemTrayClient::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {
  ++show_third_party_vpn_create_count_;
  last_third_party_vpn_extension_id_ = extension_id;
}

void TestSystemTrayClient::ShowArcVpnCreate(const std::string& app_id) {
  ++show_arc_vpn_create_count_;
  last_arc_vpn_app_id_ = app_id;
}

void TestSystemTrayClient::ShowNetworkSettings(const std::string& network_id) {
  show_network_settings_count_++;
  last_network_settings_network_id_ = network_id;
}

void TestSystemTrayClient::ShowHotspotSubpage() {
  show_hotspot_subpage_count_++;
}

void TestSystemTrayClient::ShowMultiDeviceSetup() {
  show_multi_device_setup_count_++;
}

void TestSystemTrayClient::ShowFirmwareUpdate() {
  show_firmware_update_count_++;
}

void TestSystemTrayClient::SetLocaleAndExit(
    const std::string& locale_iso_code) {}

void TestSystemTrayClient::ShowAccessCodeCastingDialog(
    AccessCodeCastDialogOpenLocation open_location) {
  ++show_access_code_casting_dialog_count_;
}

void TestSystemTrayClient::ShowCalendarEvent(
    const std::optional<GURL>& event_url,
    const base::Time& date,
    bool& opened_pwa,
    GURL& final_event_url) {
  show_calendar_event_count_++;
}

void TestSystemTrayClient::ShowVideoConference(
    const GURL& video_conference_url) {
  show_video_conference_count_++;
}

void TestSystemTrayClient::ShowChannelInfoAdditionalDetails() {
  ++show_channel_info_additional_details_count_;
}

void TestSystemTrayClient::ShowChannelInfoGiveFeedback() {
  ++show_channel_info_give_feedback_count_;
}

void TestSystemTrayClient::ShowAudioSettings() {
  ++show_audio_settings_count_;
}

bool TestSystemTrayClient::IsUserFeedbackEnabled() {
  return user_feedback_enabled_;
}

void TestSystemTrayClient::ShowEolInfoPage() {
  ++show_eol_info_count_;
}

void TestSystemTrayClient::RecordEolNoticeShown() {}

void TestSystemTrayClient::ShowGraphicsTabletSettings() {
  ++show_graphics_tablet_settings_count_;
}

void TestSystemTrayClient::ShowMouseSettings() {
  ++show_mouse_settings_count_;
}

void TestSystemTrayClient::ShowTouchpadSettings() {
  ++show_touchpad_settings_count_;
}

void TestSystemTrayClient::ShowRemapKeysSubpage(int device_id) {
  ++show_remap_keys_subpage_count_;
}

void TestSystemTrayClient::ShowYouTubeMusicPremiumPage() {
  ++show_youtube_music_premium_page_count_;
}

void TestSystemTrayClient::ShowChromebookPerksYouTubePage() {
  ++show_chromebook_perks_youtube_page_count_;
}

void TestSystemTrayClient::ShowKeyboardSettings() {
  ++show_keyboard_settings_count_;
}

void TestSystemTrayClient::ShowPointingStickSettings() {
  ++show_pointing_stick_settings_count_;
}

void TestSystemTrayClient::ShowNearbyShareSettings() {
  ++show_nearby_share_settings_count_;
}

}  // namespace ash
