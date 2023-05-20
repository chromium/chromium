// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file is the entry point for custom elements and other
 * modules that should be lazily loaded in the ChromeOS Settings frontend app.
 * This should include:
 *  - Top-level pages that exist in the "Advanced" section.
 *  - All subpages
 */

import './strings.m.js';
/** Top-level Advanced section pages */
import './crostini_page/crostini_page.js';
import './date_time_page/date_time_page.js';
import './os_files_page/os_files_page.js';
import './os_languages_page/os_languages_section.js';
import './os_printing_page/os_printing_page.js';
import './os_reset_page/os_reset_page.js';
/** Subpages */
import './internet_page/apn_subpage.js';
import './internet_page/hotspot_subpage.js';
import './internet_page/internet_detail_subpage.js';
import './internet_page/internet_known_networks_subpage.js';
import './internet_page/internet_subpage.js';
import './internet_page/passpoint_subpage.js';
import './kerberos_page/kerberos_accounts_subpage.js';
import './os_a11y_page/manage_a11y_subpage.js';
import './os_a11y_page/display_and_magnification_subpage.js';
import './os_a11y_page/keyboard_and_text_input_page.js';
import './os_a11y_page/cursor_and_touchpad_page.js';
import './os_a11y_page/chromevox_subpage.js';
import './os_a11y_page/select_to_speak_subpage.js';
import './os_a11y_page/switch_access_subpage.js';
import './os_a11y_page/text_to_speech_subpage.js';
import './os_a11y_page/tts_voice_subpage.js';
import './os_about_page/detailed_build_info_subpage.js';
import './os_search_page/google_assistant_subpage.js';
import './os_search_page/search_subpage.js';
import './os_people_page/account_manager_subpage.js';
import './os_people_page/fingerprint_list_subpage.js';
import './os_people_page/lock_screen_subpage.js';
import './os_people_page/os_sync_controls_subpage.js';
import './os_people_page/os_sync_subpage.js';
import './os_privacy_page/manage_users_subpage.js';
import './os_privacy_page/privacy_hub_subpage.js';
import './os_privacy_page/smart_privacy_subpage.js';
// TODO(b/263414034) Determine if elements below adhere to the lazy loading
// criteria and are needed here
import './crostini_page/bruschetta_subpage.js';
import './crostini_page/crostini_arc_adb.js';
import './crostini_page/crostini_arc_adb_confirmation_dialog.js';
import './crostini_page/crostini_confirmation_dialog.js';
import './crostini_page/crostini_disk_resize_confirmation_dialog.js';
import './crostini_page/crostini_disk_resize_dialog.js';
import './crostini_page/crostini_export_import.js';
import './crostini_page/crostini_extra_containers.js';
import './crostini_page/crostini_extra_containers_create_dialog.js';
import './crostini_page/crostini_import_confirmation_dialog.js';
import './crostini_page/crostini_port_forwarding.js';
import './crostini_page/crostini_port_forwarding_add_port_dialog.js';
import './crostini_page/crostini_shared_usb_devices.js';
import './crostini_page/crostini_subpage.js';
import './date_time_page/system_geolocation_dialog.js';
import './date_time_page/timezone_selector.js';
import './guest_os/guest_os_container_select.js';
import './guest_os/guest_os_shared_usb_devices.js';
import './guest_os/guest_os_shared_usb_devices_add_dialog.js';
import './guest_os/guest_os_shared_paths.js';
import './keyboard_shortcut_banner/keyboard_shortcut_banner.js';
import './os_files_page/google_drive_subpage.js';
import './os_files_page/google_drive_confirmation_dialog.js';
import './os_files_page/office_page.js';
import './os_languages_page/input_method_options_page.js';
import './os_languages_page/input_page.js';
import './os_languages_page/os_edit_dictionary_page.js';
import './os_languages_page/os_japanese_manage_user_dictionary_page.js';
import './os_languages_page/os_languages_page_v2.js';
import './os_languages_page/smart_inputs_page.js';
import './os_printing_page/cups_add_print_server_dialog.js';
import './os_printing_page/cups_add_printer_dialog.js';
import './os_printing_page/cups_add_printer_manually_dialog.js';
import './os_printing_page/cups_add_printer_manufacturer_model_dialog.js';
import './os_printing_page/cups_edit_printer_dialog.js';
import './os_printing_page/cups_nearby_printers.js';
import './os_printing_page/cups_printer_dialog_error.js';
import './os_printing_page/cups_printer_shared.css.js';
import './os_printing_page/cups_printers_entry.js';
import './os_printing_page/cups_printers.js';
import './os_printing_page/cups_saved_printers.js';
import './os_printing_page/cups_settings_add_printer_dialog.js';
import './os_printing_page/printer_status.js';
import './os_reset_page/os_powerwash_dialog.js';
import './os_reset_page/os_powerwash_dialog_esim_item.js';
import './os_files_page/smb_shares_page.js';
import '/shared/settings/privacy_page/secure_dns.js';
import '/shared/settings/privacy_page/secure_dns_input.js';

/**
 * With the optimize_webui() build step, the generated JS files are bundled
 * into a single JS file. The exports below are necessary so they can be
 * imported into browser tests.
 */
export {SettingsRadioGroupElement} from '/shared/settings/controls/settings_radio_group.js';
export {LifetimeBrowserProxyImpl} from '/shared/settings/lifetime_browser_proxy.js';
export {AddSmbShareDialogElement} from 'chrome://resources/ash/common/smb_shares/add_smb_share_dialog.js';
export {SmbBrowserProxy, SmbBrowserProxyImpl, SmbMountResult} from 'chrome://resources/ash/common/smb_shares/smb_browser_proxy.js';
export {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_page/crostini_browser_proxy.js';
export {TimeZoneAutoDetectMethod} from './date_time_page/date_time_types.js';
export {TimeZoneBrowserProxyImpl} from './date_time_page/timezone_browser_proxy.js';
export {TimezoneSelectorElement} from './date_time_page/timezone_selector.js';
export {TimezoneSubpageElement} from './date_time_page/timezone_subpage.js';
export {CROSTINI_TYPE, GuestOsBrowserProxy, GuestOsBrowserProxyImpl, GuestOsSharedUsbDevice, PLUGIN_VM_TYPE} from './guest_os/guest_os_browser_proxy.js';
export {SettingsGuestOsSharedPathsElement} from './guest_os/guest_os_shared_paths.js';
export {SettingsGuestOsSharedUsbDevicesElement} from './guest_os/guest_os_shared_usb_devices.js';
export {SettingsPasspointSubpageElement} from './internet_page/passpoint_subpage.js';
export {TetherConnectionDialogElement} from './internet_page/tether_connection_dialog.js';
export {KerberosAccount, KerberosAccountsBrowserProxy, KerberosAccountsBrowserProxyImpl, KerberosConfigErrorCode, KerberosErrorType, ValidateKerberosConfigResult} from './kerberos_page/kerberos_accounts_browser_proxy.js';
export {KeyboardShortcutBanner} from './keyboard_shortcut_banner/keyboard_shortcut_banner.js';
export {SettingsMultideviceCombinedSetupItemElement} from './multidevice_page/multidevice_combined_setup_item.js';
export {SettingsMultideviceFeatureItemElement} from './multidevice_page/multidevice_feature_item.js';
export {SettingsMultideviceFeatureToggleElement} from './multidevice_page/multidevice_feature_toggle.js';
export {SettingsMultideviceSmartlockItemElement} from './multidevice_page/multidevice_smartlock_item.js';
export {SettingsMultideviceTaskContinuationDisabledLinkElement} from './multidevice_page/multidevice_task_continuation_disabled_link.js';
export {SettingsMultideviceTaskContinuationItemElement} from './multidevice_page/multidevice_task_continuation_item.js';
export {SettingsMultideviceWifiSyncDisabledLinkElement} from './multidevice_page/multidevice_wifi_sync_disabled_link.js';
export {SettingsAudioAndCaptionsPageElement} from './os_a11y_page/audio_and_captions_page.js';
export {BluetoothBrailleDisplayListener, BluetoothBrailleDisplayManager} from './os_a11y_page/bluetooth_braille_display_manager.js';
export {BluetoothBrailleDisplayUiElement} from './os_a11y_page/bluetooth_braille_display_ui.js';
export {ChangeDictationLocaleDialog, DictationLocaleOption} from './os_a11y_page/change_dictation_locale_dialog.js';
export {SettingsChromeVoxSubpageElement} from './os_a11y_page/chromevox_subpage.js';
export {SettingsCursorAndTouchpadPageElement} from './os_a11y_page/cursor_and_touchpad_page.js';
export {SettingsDisplayAndMagnificationSubpageElement} from './os_a11y_page/display_and_magnification_subpage.js';
export {SettingsKeyboardAndTextInputPageElement} from './os_a11y_page/keyboard_and_text_input_page.js';
export {SettingsManageA11ySubpageElement} from './os_a11y_page/manage_a11y_subpage.js';
export {SettingsSwitchAccessActionAssignmentDialogElement} from './os_a11y_page/switch_access_action_assignment_dialog.js';
export {SwitchAccessCommand} from './os_a11y_page/switch_access_constants.js';
export {PdfOcrUserSelection, SettingsTextToSpeechSubpageElement} from './os_a11y_page/text_to_speech_subpage.js';
export {SettingsTtsVoiceSubpageElement} from './os_a11y_page/tts_voice_subpage.js';
export {SettingsGoogleDriveSubpageElement} from './os_files_page/google_drive_subpage.js';
export {SettingsOfficePageElement} from './os_files_page/office_page.js';
export {OsSettingsFilesPageElement} from './os_files_page/os_files_page.js';
export {SettingsSmbSharesPageElement} from './os_files_page/smb_shares_page.js';
export {SettingsInputMethodOptionsPageElement} from './os_languages_page/input_method_options_page.js';
export {LanguagesBrowserProxyImpl} from './os_languages_page/languages_browser_proxy.js';
export {InputsShortcutReminderState, LanguagesMetricsProxyImpl, LanguagesPageInteraction} from './os_languages_page/languages_metrics_proxy.js';
export {LanguageState} from './os_languages_page/languages_types.js';
export {OsSettingsClearPersonalizedDataDialogElement} from './os_languages_page/os_japanese_clear_ime_data_dialog.js';
export {OsSettingsSmartInputsPageElement} from './os_languages_page/smart_inputs_page.js';
export {Account, AccountManagerBrowserProxy, AccountManagerBrowserProxyImpl} from './os_people_page/account_manager_browser_proxy.js';
export {SettingsUsersAddUserDialogElement} from './os_people_page/add_user_dialog.js';
export {FingerprintBrowserProxy, FingerprintBrowserProxyImpl, FingerprintInfo, FingerprintResultType} from './os_people_page/fingerprint_browser_proxy.js';
export {SettingsFingerprintListSubpageElement} from './os_people_page/fingerprint_list_subpage.js';
export {SettingsLockScreenElement} from './os_people_page/lock_screen_subpage.js';
export {OsSyncBrowserProxy, OsSyncBrowserProxyImpl, OsSyncPrefs} from './os_people_page/os_sync_browser_proxy.js';
export {FingerprintSetupStep, SettingsSetupFingerprintDialogElement} from './os_people_page/setup_fingerprint_dialog.js';
export {PrinterListEntry, PrinterType} from './os_printing_page/cups_printer_types.js';
export {CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, PrinterSetupResult, PrintServerResult} from './os_printing_page/cups_printers_browser_proxy.js';
export {SettingsCupsPrintersEntryElement} from './os_printing_page/cups_printers_entry.js';
export {CupsPrintersEntryManager} from './os_printing_page/cups_printers_entry_manager.js';
export {OsSettingsPrintingPageElement} from './os_printing_page/os_printing_page.js';
export {computePrinterState, getStatusReasonFromPrinterStatus, PrinterState, PrinterStatusReason, PrinterStatusSeverity} from './os_printing_page/printer_status.js';
export {MediaDevicesProxy} from './os_privacy_page/media_devices_proxy.js';
export {PrivacyHubBrowserProxy, PrivacyHubBrowserProxyImpl} from './os_privacy_page/privacy_hub_browser_proxy.js';
export {SettingsPrivacyHubSubpage} from './os_privacy_page/privacy_hub_subpage.js';
export {SettingsSmartPrivacySubpage} from './os_privacy_page/smart_privacy_subpage.js';
export {OsResetBrowserProxyImpl} from './os_reset_page/os_reset_browser_proxy.js';
export {GoogleAssistantBrowserProxy, GoogleAssistantBrowserProxyImpl} from './os_search_page/google_assistant_browser_proxy.js';
export {ConsentStatus, DspHotwordState, SettingsGoogleAssistantSubpageElement} from './os_search_page/google_assistant_subpage.js';
export {SettingsSearchSubpageElement} from './os_search_page/search_subpage.js';
