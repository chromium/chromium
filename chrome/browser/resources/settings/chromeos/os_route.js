// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import * as routesMojomWebui from '../mojom-webui/routes.mojom-webui.js';
import {Route, Router} from './router.js';

import {OsSettingsRoutes} from './os_settings_routes.js';

const {Section, Subpage} = routesMojomWebui;

/**
 * @param {!Route} parent
 * @param {string} path
 * @param {!Section} section
 * @return {!Route}
 */
function createSection(parent, path, section) {
  // TODO(khorimoto): Add |section| to the the Route object.
  return parent.createSection('/' + path, /*section=*/ path);
}

/**
 * @param {!Route} parent
 * @param {string} path
 * @param {!Subpage} subpage
 * @return {!Route}
 */
function createSubpage(parent, path, subpage) {
  // TODO(khorimoto): Add |subpage| to the Route object.
  return parent.createChild('/' + path);
}

/**
 * Creates Route objects for each path corresponding to CrOS settings content.
 * @return {!OsSettingsRoutes}
 */
function createOSSettingsRoutes() {
  const r = /** @type {!OsSettingsRoutes} */ ({});

  // Special routes: BASIC is the main page which loads if no path is
  // provided, ADVANCED is the bottom section of the main page which is not
  // visible unless the user enables it, and OS_SIGN_OUT is a sign out dialog.
  r.BASIC = new Route('/');
  r.ADVANCED = new Route('/advanced');
  r.OS_SIGN_OUT = r.BASIC.createChild('/osSignOut');
  r.OS_SIGN_OUT.isNavigableDialog = true;

  // Network section.
  r.INTERNET = createSection(
      r.BASIC, routesMojomWebui.NETWORK_SECTION_PATH, Section.kNetwork);
  // Note: INTERNET_NETWORKS and NETWORK_DETAIL are special cases because they
  // includes several subpages, one per network type. Default to kWifiNetworks
  // and kWifiDetails subpages.
  r.INTERNET_NETWORKS =
      createSubpage(r.INTERNET, 'networks', Subpage.kWifiNetworks);
  r.NETWORK_DETAIL =
      createSubpage(r.INTERNET, 'networkDetail', Subpage.kWifiDetails);
  r.KNOWN_NETWORKS = createSubpage(
      r.INTERNET, routesMojomWebui.KNOWN_NETWORKS_SUBPAGE_PATH,
      Subpage.kKnownNetworks);
  if (loadTimeData.getBoolean('isHotspotEnabled')) {
    r.HOTSPOT_DETAIL =
        createSubpage(r.INTERNET, 'hotspotDetail', Subpage.kHotspotDetails);
  }
  if (loadTimeData.getBoolean('isApnRevampEnabled')) {
    r.APN = createSubpage(
        r.INTERNET, routesMojomWebui.APN_SUBPAGE_PATH, Subpage.kApn);
  }

  // Bluetooth section.
  r.BLUETOOTH = createSection(
      r.BASIC, routesMojomWebui.BLUETOOTH_SECTION_PATH, Section.kBluetooth);
  r.BLUETOOTH_DEVICES = createSubpage(
      r.BLUETOOTH, routesMojomWebui.BLUETOOTH_DEVICES_SUBPAGE_PATH,
      Subpage.kBluetoothDevices);
  r.BLUETOOTH_DEVICE_DETAIL = createSubpage(
      r.BLUETOOTH, routesMojomWebui.BLUETOOTH_DEVICE_DETAIL_SUBPAGE_PATH,
      Subpage.kBluetoothDeviceDetail);
  if (loadTimeData.getBoolean('enableSavedDevicesFlag')) {
    r.BLUETOOTH_SAVED_DEVICES = createSubpage(
        r.BLUETOOTH, routesMojomWebui.BLUETOOTH_SAVED_DEVICES_SUBPAGE_PATH,
        Subpage.kBluetoothSavedDevices);
  }

  // MultiDevice section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.MULTIDEVICE = createSection(
        r.BASIC, routesMojomWebui.MULTI_DEVICE_SECTION_PATH,
        Section.kMultiDevice);
    r.MULTIDEVICE_FEATURES = createSubpage(
        r.MULTIDEVICE, routesMojomWebui.MULTI_DEVICE_FEATURES_SUBPAGE_PATH,
        Subpage.kMultiDeviceFeatures);
    if (loadTimeData.getBoolean('isNearbyShareSupported')) {
      r.NEARBY_SHARE = createSubpage(
          r.MULTIDEVICE, routesMojomWebui.NEARBY_SHARE_SUBPAGE_PATH,
          Subpage.kNearbyShare);
    }
  }

  // People section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.OS_PEOPLE = createSection(
        r.BASIC, routesMojomWebui.PEOPLE_SECTION_PATH, Section.kPeople);
    r.ACCOUNT_MANAGER = createSubpage(
        r.OS_PEOPLE, routesMojomWebui.MY_ACCOUNTS_SUBPAGE_PATH,
        Subpage.kMyAccounts);
    r.OS_SYNC = createSubpage(
        r.OS_PEOPLE, routesMojomWebui.SYNC_SUBPAGE_PATH, Subpage.kSync);
    r.SYNC = createSubpage(
        r.OS_PEOPLE, routesMojomWebui.SYNC_SETUP_SUBPAGE_PATH,
        Subpage.kSyncSetup);
  }

  // Kerberos section.
  if (loadTimeData.valueExists('isKerberosEnabled') &&
      loadTimeData.getBoolean('isKerberosEnabled')) {
    r.KERBEROS = createSection(
        r.BASIC, routesMojomWebui.KERBEROS_SECTION_PATH, Section.kKerberos);
    r.KERBEROS_ACCOUNTS_V2 = createSubpage(
        r.KERBEROS, routesMojomWebui.KERBEROS_ACCOUNTS_V2_SUBPAGE_PATH,
        Subpage.kKerberosAccountsV2);
  }

  // Device section.
  r.DEVICE = createSection(
      r.BASIC, routesMojomWebui.DEVICE_SECTION_PATH, Section.kDevice);
  r.POINTERS = createSubpage(
      r.DEVICE, routesMojomWebui.POINTERS_SUBPAGE_PATH, Subpage.kPointers);
  r.KEYBOARD = createSubpage(
      r.DEVICE, routesMojomWebui.KEYBOARD_SUBPAGE_PATH, Subpage.kKeyboard);
  r.STYLUS = createSubpage(
      r.DEVICE, routesMojomWebui.STYLUS_SUBPAGE_PATH, Subpage.kStylus);
  r.DISPLAY = createSubpage(
      r.DEVICE, routesMojomWebui.DISPLAY_SUBPAGE_PATH, Subpage.kDisplay);
  if (loadTimeData.getBoolean('enableAudioSettingsPage')) {
    r.AUDIO = createSubpage(
        r.DEVICE, routesMojomWebui.AUDIO_SUBPAGE_PATH, Subpage.kAudio);
  }
  if (loadTimeData.getBoolean('enableInputDeviceSettingsSplit')) {
    r.PER_DEVICE_KEYBOARD = createSubpage(
        r.DEVICE, routesMojomWebui.PER_DEVICE_KEYBOARD_SUBPAGE_PATH,
        Subpage.kPerDeviceKeyboard);
  }
  r.STORAGE = createSubpage(
      r.DEVICE, routesMojomWebui.STORAGE_SUBPAGE_PATH, Subpage.kStorage);
  r.EXTERNAL_STORAGE_PREFERENCES = createSubpage(
      r.STORAGE, routesMojomWebui.EXTERNAL_STORAGE_SUBPAGE_PATH,
      Subpage.kExternalStorage);
  r.POWER = createSubpage(
      r.DEVICE, routesMojomWebui.POWER_SUBPAGE_PATH, Subpage.kPower);

  // Personalization section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.PERSONALIZATION = createSection(
        r.BASIC, routesMojomWebui.PERSONALIZATION_SECTION_PATH,
        Section.kPersonalization);
  }

  // Search and Assistant section.
  r.OS_SEARCH = createSection(
      r.BASIC, routesMojomWebui.SEARCH_AND_ASSISTANT_SECTION_PATH,
      Section.kSearchAndAssistant);
  r.GOOGLE_ASSISTANT = createSubpage(
      r.OS_SEARCH, routesMojomWebui.ASSISTANT_SUBPAGE_PATH, Subpage.kAssistant);
  r.SEARCH_SUBPAGE = createSubpage(
      r.OS_SEARCH, routesMojomWebui.SEARCH_SUBPAGE_PATH, Subpage.kSearch);

  // Apps section.
  r.APPS =
      createSection(r.BASIC, routesMojomWebui.APPS_SECTION_PATH, Section.kApps);
  r.APP_NOTIFICATIONS = createSubpage(
      r.APPS, routesMojomWebui.APP_NOTIFICATIONS_SUBPAGE_PATH,
      Subpage.kAppNotifications);
  r.APP_MANAGEMENT = createSubpage(
      r.APPS, routesMojomWebui.APP_MANAGEMENT_SUBPAGE_PATH,
      Subpage.kAppManagement);
  r.APP_MANAGEMENT_DETAIL = createSubpage(
      r.APP_MANAGEMENT, routesMojomWebui.APP_DETAILS_SUBPAGE_PATH,
      Subpage.kAppDetails);
  if (loadTimeData.valueExists('androidAppsVisible') &&
      loadTimeData.getBoolean('androidAppsVisible')) {
    r.ANDROID_APPS_DETAILS = createSubpage(
        r.APPS, routesMojomWebui.GOOGLE_PLAY_STORE_SUBPAGE_PATH,
        Subpage.kGooglePlayStore);
    if (loadTimeData.valueExists('showArcvmManageUsb') &&
        loadTimeData.getBoolean('showArcvmManageUsb')) {
      r.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES = createSubpage(
          r.ANDROID_APPS_DETAILS,
          routesMojomWebui.ARC_VM_USB_PREFERENCES_SUBPAGE_PATH,
          Subpage.kArcVmUsbPreferences);
    }
  }
  if (loadTimeData.valueExists('showPluginVm') &&
      loadTimeData.getBoolean('showPluginVm')) {
    r.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS = createSubpage(
        r.APP_MANAGEMENT, routesMojomWebui.PLUGIN_VM_SHARED_PATHS_SUBPAGE_PATH,
        Subpage.kPluginVmSharedPaths);
    r.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES = createSubpage(
        r.APP_MANAGEMENT,
        routesMojomWebui.PLUGIN_VM_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kPluginVmUsbPreferences);
  }

  // Accessibility section.
  r.OS_ACCESSIBILITY = createSection(
      r.BASIC, routesMojomWebui.ACCESSIBILITY_SECTION_PATH,
      Section.kAccessibility);
  r.MANAGE_ACCESSIBILITY = createSubpage(
      r.OS_ACCESSIBILITY, routesMojomWebui.MANAGE_ACCESSIBILITY_SUBPAGE_PATH,
      Subpage.kManageAccessibility);
  r.A11Y_TEXT_TO_SPEECH = createSubpage(
      r.OS_ACCESSIBILITY, routesMojomWebui.TEXT_TO_SPEECH_PAGE_PATH,
      Subpage.kTextToSpeechPage);
  r.A11Y_DISPLAY_AND_MAGNIFICATION = createSubpage(
      r.OS_ACCESSIBILITY,
      routesMojomWebui.DISPLAY_AND_MAGNIFICATION_SUBPAGE_PATH,
      Subpage.kDisplayAndMagnification);
  r.A11Y_KEYBOARD_AND_TEXT_INPUT = createSubpage(
      r.OS_ACCESSIBILITY, routesMojomWebui.KEYBOARD_AND_TEXT_INPUT_SUBPAGE_PATH,
      Subpage.kKeyboardAndTextInput);
  r.A11Y_CURSOR_AND_TOUCHPAD = createSubpage(
      r.OS_ACCESSIBILITY, routesMojomWebui.CURSOR_AND_TOUCHPAD_SUBPAGE_PATH,
      Subpage.kCursorAndTouchpad);
  r.A11Y_AUDIO_AND_CAPTIONS = createSubpage(
      r.OS_ACCESSIBILITY, routesMojomWebui.AUDIO_AND_CAPTIONS_SUBPAGE_PATH,
      Subpage.kAudioAndCaptions);
  if (loadTimeData.valueExists(
          'isAccessibilitySelectToSpeakPageMigrationEnabled') &&
      loadTimeData.getBoolean(
          'isAccessibilitySelectToSpeakPageMigrationEnabled')) {
    r.A11Y_SELECT_TO_SPEAK = createSubpage(
        r.A11Y_TEXT_TO_SPEECH, routesMojomWebui.SELECT_TO_SPEAK_SUBPAGE_PATH,
        Subpage.kSelectToSpeak);
  }
  r.MANAGE_TTS_SETTINGS = createSubpage(
      loadTimeData.getBoolean('isKioskModeActive') ? r.MANAGE_ACCESSIBILITY :
                                                     r.A11Y_TEXT_TO_SPEECH,
      routesMojomWebui.TEXT_TO_SPEECH_SUBPAGE_PATH, Subpage.kTextToSpeech);
  r.MANAGE_SWITCH_ACCESS_SETTINGS = createSubpage(
      r.A11Y_KEYBOARD_AND_TEXT_INPUT,
      routesMojomWebui.SWITCH_ACCESS_OPTIONS_SUBPAGE_PATH,
      Subpage.kSwitchAccessOptions);

  // Crostini section.
  r.CROSTINI = createSection(
      r.ADVANCED, routesMojomWebui.CROSTINI_SECTION_PATH, Section.kCrostini);
  if (loadTimeData.valueExists('showCrostini') &&
      loadTimeData.getBoolean('showCrostini')) {
    r.CROSTINI_DETAILS = createSubpage(
        r.CROSTINI, routesMojomWebui.CROSTINI_DETAILS_SUBPAGE_PATH,
        Subpage.kCrostiniDetails);
    r.CROSTINI_SHARED_PATHS = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojomWebui.CROSTINI_MANAGE_SHARED_FOLDERS_SUBPAGE_PATH,
        Subpage.kCrostiniManageSharedFolders);
    r.CROSTINI_SHARED_USB_DEVICES = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojomWebui.CROSTINI_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kCrostiniUsbPreferences);
    if (loadTimeData.valueExists('showCrostiniExportImport') &&
        loadTimeData.getBoolean('showCrostiniExportImport')) {
      r.CROSTINI_EXPORT_IMPORT = createSubpage(
          r.CROSTINI_DETAILS,
          routesMojomWebui.CROSTINI_BACKUP_AND_RESTORE_SUBPAGE_PATH,
          Subpage.kCrostiniBackupAndRestore);
    }
    if (loadTimeData.valueExists('showCrostiniExtraContainers') &&
        loadTimeData.getBoolean('showCrostiniExtraContainers')) {
      r.CROSTINI_EXTRA_CONTAINERS = createSubpage(
          r.CROSTINI_DETAILS,
          routesMojomWebui.CROSTINI_EXTRA_CONTAINERS_SUBPAGE_PATH,
          Subpage.kCrostiniExtraContainers);
    }

    r.CROSTINI_ANDROID_ADB = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojomWebui.CROSTINI_DEVELOP_ANDROID_APPS_SUBPAGE_PATH,
        Subpage.kCrostiniDevelopAndroidApps);
    r.CROSTINI_PORT_FORWARDING = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojomWebui.CROSTINI_PORT_FORWARDING_SUBPAGE_PATH,
        Subpage.kCrostiniPortForwarding);

    r.BRUSCHETTA_DETAILS = createSubpage(
        r.CROSTINI, routesMojomWebui.BRUSCHETTA_DETAILS_SUBPAGE_PATH,
        Subpage.kBruschettaDetails);
    r.BRUSCHETTA_SHARED_USB_DEVICES = createSubpage(
        r.BRUSCHETTA_DETAILS,
        routesMojomWebui.BRUSCHETTA_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kBruschettaUsbPreferences);
    r.BRUSCHETTA_SHARED_PATHS = createSubpage(
        r.BRUSCHETTA_DETAILS,
        routesMojomWebui.BRUSCHETTA_MANAGE_SHARED_FOLDERS_SUBPAGE_PATH,
        Subpage.kBruschettaManageSharedFolders);
  }

  // Date and Time section.
  r.DATETIME = createSection(
      r.ADVANCED, routesMojomWebui.DATE_AND_TIME_SECTION_PATH,
      Section.kDateAndTime);
  r.DATETIME_TIMEZONE_SUBPAGE = createSubpage(
      r.DATETIME, routesMojomWebui.TIME_ZONE_SUBPAGE_PATH, Subpage.kTimeZone);

  // Privacy and Security section.
  r.OS_PRIVACY = createSection(
      r.BASIC, routesMojomWebui.PRIVACY_AND_SECURITY_SECTION_PATH,
      Section.kPrivacyAndSecurity);
  r.LOCK_SCREEN = createSubpage(
      r.OS_PRIVACY, routesMojomWebui.SECURITY_AND_SIGN_IN_SUBPAGE_PATH_V2,
      Subpage.kSecurityAndSignInV2);
  r.FINGERPRINT = createSubpage(
      r.LOCK_SCREEN, routesMojomWebui.FINGERPRINT_SUBPAGE_PATH_V2,
      Subpage.kFingerprintV2);
  r.ACCOUNTS = createSubpage(
      r.OS_PRIVACY, routesMojomWebui.MANAGE_OTHER_PEOPLE_SUBPAGE_PATH_V2,
      Subpage.kManageOtherPeopleV2);
  r.SMART_PRIVACY = createSubpage(
      r.OS_PRIVACY, routesMojomWebui.SMART_PRIVACY_SUBPAGE_PATH,
      Subpage.kSmartPrivacy);
  r.PRIVACY_HUB = createSubpage(
      r.OS_PRIVACY, routesMojomWebui.PRIVACY_HUB_SUBPAGE_PATH,
      Subpage.kPrivacyHub);

  // Languages and Input section.
  r.OS_LANGUAGES = createSection(
      r.ADVANCED, routesMojomWebui.LANGUAGES_AND_INPUT_SECTION_PATH,
      Section.kLanguagesAndInput);
  r.OS_LANGUAGES_LANGUAGES = createSubpage(
      r.OS_LANGUAGES, routesMojomWebui.LANGUAGES_SUBPAGE_PATH,
      Subpage.kLanguages);
  r.OS_LANGUAGES_INPUT = createSubpage(
      r.OS_LANGUAGES, routesMojomWebui.INPUT_SUBPAGE_PATH, Subpage.kInput);
  r.OS_LANGUAGES_INPUT_METHOD_OPTIONS = createSubpage(
      r.OS_LANGUAGES_INPUT, routesMojomWebui.INPUT_METHOD_OPTIONS_SUBPAGE_PATH,
      Subpage.kInputMethodOptions);
  r.OS_LANGUAGES_EDIT_DICTIONARY = createSubpage(
      r.OS_LANGUAGES_INPUT, routesMojomWebui.EDIT_DICTIONARY_SUBPAGE_PATH,
      Subpage.kEditDictionary);
  r.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY = createSubpage(
      r.OS_LANGUAGES_INPUT,
      routesMojomWebui.JAPANESE_MANAGE_USER_DICTIONARY_SUBPAGE_PATH,
      Subpage.kJapaneseManageUserDictionary);
  r.OS_LANGUAGES_SMART_INPUTS = createSubpage(
      r.OS_LANGUAGES, routesMojomWebui.SMART_INPUTS_SUBPAGE_PATH,
      Subpage.kSmartInputs);


  // Files section.
  if (!loadTimeData.getBoolean('isGuest')) {
    r.FILES = createSection(
        r.ADVANCED, routesMojomWebui.FILES_SECTION_PATH, Section.kFiles);
    r.SMB_SHARES = createSubpage(
        r.FILES, routesMojomWebui.NETWORK_FILE_SHARES_SUBPAGE_PATH,
        Subpage.kNetworkFileShares);
  }

  // Printing section.
  r.OS_PRINTING = createSection(
      r.ADVANCED, routesMojomWebui.PRINTING_SECTION_PATH, Section.kPrinting);
  r.CUPS_PRINTERS = createSubpage(
      r.OS_PRINTING, routesMojomWebui.PRINTING_DETAILS_SUBPAGE_PATH,
      Subpage.kPrintingDetails);

  // Reset section.
  if (loadTimeData.valueExists('allowPowerwash') &&
      loadTimeData.getBoolean('allowPowerwash')) {
    r.OS_RESET = createSection(
        r.ADVANCED, routesMojomWebui.RESET_SECTION_PATH, Section.kReset);
  }

  // About section. Note that this section is a special case, since it is not
  // part of the main page. In this case, the "About Chrome OS" subpage is
  // implemented using createSection().
  // TODO(khorimoto): Add Section.kAboutChromeOs to Route object.
  r.ABOUT = new Route('/' + routesMojomWebui.ABOUT_CHROME_OS_SECTION_PATH);
  r.ABOUT_ABOUT = r.ABOUT.createSection(
      '/' + routesMojomWebui.ABOUT_CHROME_OS_DETAILS_SUBPAGE_PATH, 'about');
  r.DETAILED_BUILD_INFO = createSubpage(
      r.ABOUT_ABOUT, routesMojomWebui.DETAILED_BUILD_INFO_SUBPAGE_PATH,
      Subpage.kDetailedBuildInfo);

  return r;
}

/**
 * @return {!Router} A router with at least those routes common to OS
 *     and browser settings. If the window is not in OS settings (based on
 *     loadTimeData) then browser specific routes are added. If the window is
 *     OS settings or if Chrome OS is using a consolidated settings page for
 *     OS and browser settings then OS specific routes are added.
 */
function buildRouter() {
  return new Router(createOSSettingsRoutes());
}

Router.setInstance(buildRouter());

window.addEventListener('popstate', function(event) {
  // On pop state, do not push the state onto the window.history again.
  const routerInstance = Router.getInstance();
  routerInstance.setCurrentRoute(
      routerInstance.getRouteForPath(window.location.pathname) ||
          routerInstance.getRoutes().BASIC,
      new URLSearchParams(window.location.search), true);
});

// TODO(dpapad): Change to 'get routes() {}' in export when we fix a bug in
// ChromePass that limits the syntax of what can be returned from cr.define().
export const routes =
    /** @type {!OsSettingsRoutes} */ (Router.getInstance().getRoutes());
