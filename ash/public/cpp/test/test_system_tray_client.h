// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_SYSTEM_TRAY_CLIENT_H_
#define ASH_PUBLIC_CPP_TEST_TEST_SYSTEM_TRAY_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/system_tray_client.h"
#include "base/strings/string_piece.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// A SystemTrayClient that does nothing. Used by AshTestBase.
class ASH_PUBLIC_EXPORT TestSystemTrayClient : public SystemTrayClient {
 public:
  TestSystemTrayClient();

  TestSystemTrayClient(const TestSystemTrayClient&) = delete;
  TestSystemTrayClient& operator=(const TestSystemTrayClient&) = delete;

  ~TestSystemTrayClient() override;

  // SystemTrayClient:
  void ShowSettings(int64_t display_id) override;
  void ShowBluetoothSettings() override;
  void ShowBluetoothSettings(const std::string& device_id) override;
  void ShowBluetoothPairingDialog(
      absl::optional<base::StringPiece> device_address) override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowDisplaySettings() override;
  void ShowDarkModeSettings() override;
  void ShowStorageSettings() override;
  void ShowPowerSettings() override;
  void ShowChromeSlow() override;
  void ShowIMESettings() override;
  void ShowConnectedDevicesSettings() override;
  void ShowTetherNetworkSettings() override;
  void ShowWifiSyncSettings() override;
  void ShowAboutChromeOS() override;
  void ShowAboutChromeOSDetails() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowGestureEducationHelp() override;
  void ShowPaletteHelp() override;
  void ShowPaletteSettings() override;
  void ShowPrivacyAndSecuritySettings() override;
  void ShowPrivacyHubSettings() override;
  void ShowSmartPrivacySettings() override;
  void ShowEnterpriseInfo() override;
  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkCreate(const std::string& type) override;
  void ShowSettingsCellularSetup(bool show_psim_flow) override;
  void ShowSettingsSimUnlock() override;
  void ShowThirdPartyVpnCreate(const std::string& extension_id) override;
  void ShowArcVpnCreate(const std::string& app_id) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  void ShowMultiDeviceSetup() override;
  void ShowFirmwareUpdate() override;
  void SetLocaleAndExit(const std::string& locale_iso_code) override;
  void ShowAccessCodeCastingDialog(
      AccessCodeCastDialogOpenLocation open_location) override;
  void ShowCalendarEvent(const absl::optional<GURL>& event_url,
                         const base::Time& date,
                         bool& opened_pwa,
                         GURL& final_event_url) override;
  void ShowGoogleMeet(const std::string& hangout_link) override;
  void ShowChannelInfoAdditionalDetails() override;
  void ShowChannelInfoGiveFeedback() override;
  void ShowAudioSettings() override;
  bool IsUserFeedbackEnabled() override;

  int show_bluetooth_settings_count() const {
    return show_bluetooth_settings_count_;
  }

  int show_network_settings_count() const {
    return show_network_settings_count_;
  }

  int show_bluetooth_pairing_dialog_count() const {
    return show_bluetooth_pairing_dialog_count_;
  }

  int show_multi_device_setup_count() const {
    return show_multi_device_setup_count_;
  }

  int show_connected_devices_settings_count() const {
    return show_connected_devices_settings_count_;
  }

  int show_os_settings_privacy_and_security_count() const {
    return show_os_settings_privacy_and_security_count_;
  }

  int show_os_settings_privacy_hub_count() const {
    return show_os_settings_privacy_hub_count_;
  }

  int show_os_smart_privacy_settings_count() const {
    return show_os_smart_privacy_settings_count_;
  }

  int show_wifi_sync_settings_count() const {
    return show_wifi_sync_settings_count_;
  }

  int show_sim_unlock_settings_count() const {
    return show_sim_unlock_settings_count_;
  }

  int show_third_party_vpn_create_count() const {
    return show_third_party_vpn_create_count_;
  }

  const std::string& last_third_party_vpn_extension_id() const {
    return last_third_party_vpn_extension_id_;
  }

  int show_arc_vpn_create_count() const { return show_arc_vpn_create_count_; }

  const std::string& last_arc_vpn_app_id() const {
    return last_arc_vpn_app_id_;
  }

  int show_network_create_count() const { return show_network_create_count_; }

  int show_access_code_casting_dialog_count() const {
    return show_access_code_casting_dialog_count_;
  }

  int show_calendar_event_count() const { return show_calendar_event_count_; }

  int show_google_meet_count() const { return show_google_meet_count_; }

  const std::string& last_network_type() const { return last_network_type_; }

  int show_firmware_update_count() const { return show_firmware_update_count_; }

  const std::string& last_bluetooth_settings_device_id() const {
    return last_bluetooth_settings_device_id_;
  }

  const std::string& last_network_settings_network_id() const {
    return last_network_settings_network_id_;
  }

  int show_channel_info_additional_details_count() const {
    return show_channel_info_additional_details_count_;
  }

  int show_channel_info_give_feedback_count() const {
    return show_channel_info_give_feedback_count_;
  }

  int show_audio_settings_count() const { return show_audio_settings_count_; }

  void set_user_feedback_enabled(bool user_feedback_enabled) {
    user_feedback_enabled_ = user_feedback_enabled;
  }

 private:
  int show_network_settings_count_ = 0;
  int show_bluetooth_settings_count_ = 0;
  int show_bluetooth_pairing_dialog_count_ = 0;
  int show_multi_device_setup_count_ = 0;
  int show_connected_devices_settings_count_ = 0;
  int show_os_settings_privacy_and_security_count_ = 0;
  int show_os_settings_privacy_hub_count_ = 0;
  int show_os_smart_privacy_settings_count_ = 0;
  int show_wifi_sync_settings_count_ = 0;
  int show_sim_unlock_settings_count_ = 0;
  int show_third_party_vpn_create_count_ = 0;
  std::string last_third_party_vpn_extension_id_;
  int show_arc_vpn_create_count_ = 0;
  std::string last_arc_vpn_app_id_;
  int show_firmware_update_count_ = 0;
  int show_network_create_count_ = 0;
  int show_access_code_casting_dialog_count_ = 0;
  int show_calendar_event_count_ = 0;
  int show_google_meet_count_ = 0;
  std::string last_bluetooth_settings_device_id_;
  std::string last_network_settings_network_id_;
  std::string last_network_type_;
  int show_channel_info_additional_details_count_ = 0;
  int show_channel_info_give_feedback_count_ = 0;
  int show_audio_settings_count_ = 0;
  bool user_feedback_enabled_ = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_SYSTEM_TRAY_CLIENT_H_
