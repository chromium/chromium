// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The main entry point for the ChromeOS Settings SWA. This
 * imports all of the necessary modules and custom elements to load the page.
 */

/** Necessary imports to load the app */
import '../strings.m.js';
import './os_settings_ui/os_settings_ui.js';
// TODO(b/263414034) Determine if these imports are needed here at all,
// or should be moved to lazy_load.ts
import '../prefs/prefs.js';
import './device_page/audio.js';
import './device_page/cros_audio_config.js';
import './device_page/device_page.js';
import './device_page/display.js';
import './device_page/display_layout.js';
import './device_page/display_overscan_dialog.js';
import './device_page/fake_input_device_data.js';
import './device_page/fake_input_device_settings_provider.js';
import './device_page/input_device_mojo_interface_provider.js';
import './device_page/input_device_settings_types.js';
import './device_page/keyboard.js';
import './device_page/per_device_keyboard.js';
import './device_page/per_device_keyboard_remap_keys.js';
import './device_page/per_device_keyboard_subsection.js';
import './device_page/per_device_mouse.js';
import './device_page/per_device_mouse_subsection.js';
import './device_page/per_device_pointing_stick.js';
import './device_page/per_device_pointing_stick_subsection.js';
import './device_page/per_device_touchpad.js';
import './device_page/per_device_touchpad_subsection.js';
import './device_page/pointers.js';
import './device_page/power.js';
import './device_page/keyboard_remap_modifier_key_row.js';
import './device_page/storage.js';
import './device_page/storage_external.js';
import './device_page/storage_external_entry.js';
import './device_page/stylus.js';
import './kerberos_page/kerberos_accounts.js';
import './kerberos_page/kerberos_page.js';
import './multidevice_page/multidevice_page.js';
import './nearby_share_page/nearby_share_receive_dialog.js';
import './nearby_share_page/nearby_share_subpage.js';
import './personalization_page/personalization_page.js';
import './os_a11y_page/change_dictation_locale_dialog.js';
import './os_a11y_page/os_a11y_page.js';
import './os_apps_page/android_apps_subpage.js';
import './os_apps_page/app_notifications_page/app_notifications_subpage.js';
import './os_apps_page/app_management_page/app_detail_view.js';
import './os_apps_page/app_management_page/app_details_item.js';
import './os_apps_page/app_management_page/app_item.js';
import './os_apps_page/app_management_page/app_management_page.js';
import './os_apps_page/app_management_page/arc_detail_view.js';
import './os_apps_page/app_management_page/borealis_page/borealis_detail_view.js';
import './os_apps_page/app_management_page/chrome_app_detail_view.js';
import './os_apps_page/app_management_page/dom_switch.js';
import './os_apps_page/app_management_page/main_view.js';
import './os_apps_page/app_management_page/pin_to_shelf_item.js';
import './os_apps_page/app_management_page/plugin_vm_page/plugin_vm_detail_view.js';
import './os_apps_page/app_management_page/pwa_detail_view.js';
import './os_apps_page/app_management_page/app_management_cros_shared_style.css.js';
import './os_apps_page/app_management_page/app_management_cros_shared_vars.css.js';
import './os_apps_page/app_management_page/supported_links_overlapping_apps_dialog.js';
import './os_apps_page/app_management_page/supported_links_dialog.js';
import './os_apps_page/app_management_page/supported_links_item.js';
import './os_apps_page/app_notifications_page/mojo_interface_provider.js';
import './os_apps_page/os_apps_page.js';
import './os_bluetooth_page/os_bluetooth_devices_subpage.js';
import './os_bluetooth_page/os_bluetooth_device_detail_subpage.js';
import './os_bluetooth_page/os_bluetooth_saved_devices_subpage.js';
import './os_bluetooth_page/os_remove_saved_device_dialog.js';
import './os_bluetooth_page/os_bluetooth_forget_device_dialog.js';
import './os_bluetooth_page/os_bluetooth_true_wireless_images.js';
import './os_bluetooth_page/os_bluetooth_change_device_name_dialog.js';
import './os_bluetooth_page/os_bluetooth_pairing_dialog.js';
import './os_bluetooth_page/os_bluetooth_page.js';
import './os_bluetooth_page/os_bluetooth_summary.js';
import './os_bluetooth_page/os_paired_bluetooth_list.js';
import './os_bluetooth_page/os_paired_bluetooth_list_item.js';
import './os_bluetooth_page/os_saved_devices_list.js';
import './os_bluetooth_page/os_saved_devices_list_item.js';
import './os_bluetooth_page/settings_fast_pair_constants.js';

import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

/**
 * With the optimize_webui() build step, the generated JS files are bundled
 * into a single JS file. The exports below are necessary so they can be
 * imported into browser tests.
 */
export * as nearbyShareMojom from '/shared/mojo/nearby_share.mojom-webui.js';
export {getContactManager, observeContactManager, setContactManagerForTesting} from '/shared/nearby_contact_manager.js';
export {getNearbyShareSettings, observeNearbyShareSettings, setNearbyShareSettingsForTesting} from '/shared/nearby_share_settings.js';
export {NearbySettings, NearbyShareSettingsMixin} from '/shared/nearby_share_settings_mixin.js';
export {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
export {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
export {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
export {SettingsDropdownMenuElement} from '../controls/settings_dropdown_menu.js';
export {SettingsSliderElement} from '../controls/settings_slider.js';
export {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
export {LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.js';
export {ProfileInfoBrowserProxyImpl} from '../people_page/profile_info_browser_proxy.js';
export {PageStatus, StatusAction, SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';
export {SettingsPrefsElement} from '../prefs/prefs.js';
export {CrSettingsPrefs} from '../prefs/prefs_types.js';
export {PrivacyPageBrowserProxyImpl, SecureDnsMode, SecureDnsUiManagementMode} from '../privacy_page/privacy_page_browser_proxy.js';
export {setCrosAudioConfigForTesting} from './device_page/cros_audio_config.js';
export {DevicePageBrowserProxy, DevicePageBrowserProxyImpl, IdleBehavior, LidClosedBehavior, NoteAppLockScreenSupport, setDisplayApiForTesting, StorageSpaceState} from './device_page/device_page_browser_proxy.js';
export * as fakeCrosAudioConfig from './device_page/fake_cros_audio_config.js';
export {fakeKeyboards, fakeKeyboards2, fakeMice, fakeMice2, fakePointingSticks, fakePointingSticks2, fakeTouchpads, fakeTouchpads2} from './device_page/fake_input_device_data.js';
export {FakeInputDeviceSettingsProvider} from './device_page/fake_input_device_settings_provider.js';
export {getInputDeviceSettingsProvider, setInputDeviceSettingsProviderForTesting, setupFakeInputDeviceSettingsProvider} from './device_page/input_device_mojo_interface_provider.js';
export {MetaKey, ModifierKey, Mouse} from './device_page/input_device_settings_types.js';
export {KeyboardRemapModifierKeyRowElement} from './device_page/keyboard_remap_modifier_key_row.js';
export {SettingsPerDeviceKeyboardElement} from './device_page/per_device_keyboard.js';
export {SettingsPerDeviceKeyboardRemapKeysElement} from './device_page/per_device_keyboard_remap_keys.js';
export {SettingsPerDeviceKeyboardSubsectionElement} from './device_page/per_device_keyboard_subsection.js';
export {SettingsPerDeviceMouseElement} from './device_page/per_device_mouse.js';
export {SettingsPerDeviceMouseSubsectionElement} from './device_page/per_device_mouse_subsection.js';
export {SettingsPerDevicePointingStickElement} from './device_page/per_device_pointing_stick.js';
export {SettingsPerDevicePointingStickSubsectionElement} from './device_page/per_device_pointing_stick_subsection.js';
export {SettingsPerDeviceTouchpadElement} from './device_page/per_device_touchpad.js';
export {SettingsPerDeviceTouchpadSubsectionElement} from './device_page/per_device_touchpad_subsection.js';
export {InternetPageBrowserProxy, InternetPageBrowserProxyImpl} from './internet_page/internet_page_browser_proxy.js';
export {KerberosAccountsBrowserProxyImpl, KerberosConfigErrorCode, KerberosErrorType} from './kerberos_page/kerberos_accounts_browser_proxy.js';
export {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from './metrics_recorder.js';
export * as appNotificationHandlerMojom from './mojom-webui/app_notification_handler.mojom-webui.js';
export * as crosAudioConfigMojom from './mojom-webui/cros_audio_config.mojom-webui.js';
export * as routesMojom from './mojom-webui/routes.mojom-webui.js';
export * as personalizationSearchMojom from './mojom-webui/search/personalization_search.mojom-webui.js';
export * as searchMojom from './mojom-webui/search/search.mojom-webui.js';
export * as searchResultIconMojom from './mojom-webui/search/search_result_icon.mojom-webui.js';
export * as userActionRecorderMojom from './mojom-webui/search/user_action_recorder.mojom-webui.js';
export * as settingMojom from './mojom-webui/setting.mojom-webui.js';
export {MultiDeviceBrowserProxy, MultiDeviceBrowserProxyImpl} from './multidevice_page/multidevice_browser_proxy.js';
export {MultiDeviceFeature, MultiDeviceFeatureState, MultiDevicePageContentData, MultiDeviceSettingsMode, PhoneHubFeatureAccessProhibitedReason, PhoneHubFeatureAccessStatus, PhoneHubPermissionsSetupMode, SmartLockSignInEnabledState} from './multidevice_page/multidevice_constants.js';
export {NotificationAccessSetupOperationStatus} from './multidevice_page/multidevice_notification_access_setup_dialog.js';
export {PermissionsSetupStatus, SetupFlowStatus} from './multidevice_page/multidevice_permissions_setup_dialog.js';
export {Account, NearbyAccountManagerBrowserProxy, NearbyAccountManagerBrowserProxyImpl} from './nearby_share_page/nearby_account_manager_browser_proxy.js';
export {getReceiveManager, observeReceiveManager, setReceiveManagerForTesting} from './nearby_share_page/nearby_share_receive_manager.js';
export {dataUsageStringToEnum, NearbyShareDataUsage} from './nearby_share_page/types.js';
export {ChromeVoxSubpageBrowserProxy, ChromeVoxSubpageBrowserProxyImpl} from './os_a11y_page/chromevox_subpage_browser_proxy.js';
export {ManageA11yPageBrowserProxy, ManageA11yPageBrowserProxyImpl} from './os_a11y_page/manage_a11y_page_browser_proxy.js';
export {OsA11yPageBrowserProxy, OsA11yPageBrowserProxyImpl} from './os_a11y_page/os_a11y_page_browser_proxy.js';
export {SelectToSpeakSubpageBrowserProxy, SelectToSpeakSubpageBrowserProxyImpl} from './os_a11y_page/select_to_speak_subpage_browser_proxy.js';
export {SwitchAccessSubpageBrowserProxy, SwitchAccessSubpageBrowserProxyImpl} from './os_a11y_page/switch_access_subpage_browser_proxy.js';
export {TextToSpeechPageBrowserProxy, TextToSpeechPageBrowserProxyImpl} from './os_a11y_page/text_to_speech_page_browser_proxy.js';
export {TtsSubpageBrowserProxy, TtsSubpageBrowserProxyImpl} from './os_a11y_page/tts_subpage_browser_proxy.js';
export {AboutPageBrowserProxyImpl, BrowserChannel, UpdateStatus} from './os_about_page/about_page_browser_proxy.js';
export {DeviceNameBrowserProxyImpl} from './os_about_page/device_name_browser_proxy.js';
export {DeviceNameState, SetDeviceNameResult} from './os_about_page/device_name_util.js';
export {AndroidAppsBrowserProxyImpl} from './os_apps_page/android_apps_browser_proxy.js';
export {addApp, changeApp, removeApp, updateSelectedAppId} from './os_apps_page/app_management_page/actions.js';
export {AppManagementBrowserProxy, AppManagementComponentBrowserProxy} from './os_apps_page/app_management_page/browser_proxy.js';
export {FakePageHandler} from './os_apps_page/app_management_page/fake_page_handler.js';
export {PluginVmBrowserProxyImpl} from './os_apps_page/app_management_page/plugin_vm_page/plugin_vm_browser_proxy.js';
export {reduceAction, updateApps} from './os_apps_page/app_management_page/reducers.js';
export {AppManagementStore} from './os_apps_page/app_management_page/store.js';
export {AppManagementStoreMixin} from './os_apps_page/app_management_page/store_mixin.js';
export {setAppNotificationProviderForTesting} from './os_apps_page/app_notifications_page/mojo_interface_provider.js';
export {OsBluetoothDevicesSubpageBrowserProxy, OsBluetoothDevicesSubpageBrowserProxyImpl} from './os_bluetooth_page/os_bluetooth_devices_subpage_browser_proxy.js';
export {FastPairSavedDevice, FastPairSavedDevicesOptInStatus} from './os_bluetooth_page/settings_fast_pair_constants.js';
export {osPageVisibility} from './os_page_visibility.js';
export {MetricsConsentBrowserProxy, MetricsConsentBrowserProxyImpl, MetricsConsentState} from './os_privacy_page/metrics_consent_browser_proxy.js';
export {DataAccessPolicyState, PeripheralDataAccessBrowserProxy, PeripheralDataAccessBrowserProxyImpl} from './os_privacy_page/peripheral_data_access_browser_proxy.js';
export {OsResetBrowserProxyImpl} from './os_reset_page/os_reset_browser_proxy.js';
export {SearchEngine, SearchEnginesBrowserProxy, SearchEnginesBrowserProxyImpl, SearchEnginesInfo} from './os_search_page/search_engines_browser_proxy.js';
export {OsSettingsMainElement} from './os_settings_main/os_settings_main.js';
export {OsSettingsPageElement} from './os_settings_page/os_settings_page.js';
export {OsSettingsSectionElement} from './os_settings_page/os_settings_section.js';
export {routes} from './os_settings_routes.js';
export {OsSettingsSearchBoxBrowserProxyImpl} from './os_settings_search_box/os_settings_search_box_browser_proxy.js';
export {OsSettingsUiElement} from './os_settings_ui/os_settings_ui.js';
export {ParentalControlsBrowserProxy, ParentalControlsBrowserProxyImpl} from './parental_controls_page/parental_controls_browser_proxy.js';
export {SettingsParentalControlsPageElement} from './parental_controls_page/parental_controls_page.js';
export {PersonalizationHubBrowserProxy, PersonalizationHubBrowserProxyImpl} from './personalization_page/personalization_hub_browser_proxy.js';
export {Route, Router} from './router.js';
export {getPersonalizationSearchHandler, setPersonalizationSearchHandlerForTesting} from './search/personalization_search_handler.js';
export {getSettingsSearchHandler, setSettingsSearchHandlerForTesting} from './search/settings_search_handler.js';

// TODO(b/257329722) After the Jelly experiment is launched, add the CSS link
// element directly to the HTML.
const jellyEnabled = loadTimeData.getBoolean('isJellyEnabled');
if (jellyEnabled) {
  const link = document.createElement('link');
  link.rel = 'stylesheet';
  link.href = 'chrome://theme/colors.css?sets=legacy,sys';
  document.head.appendChild(link);
  document.body.classList.add('jelly-enabled');
}

window.onload = () => {
  if (jellyEnabled) {
    startColorChangeUpdater();
  }
};
