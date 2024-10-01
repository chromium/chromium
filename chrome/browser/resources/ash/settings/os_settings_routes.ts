// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Defines the class for navigable routes. Also exports a function which
 * creates the set of routes available, based on loadTimeData, and is meant to
 * be used when initializing the Router instance. Routes should be derived from
 * the Router singleton instance, rather than imported from here.
 */

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {androidAppsVisible, isAppParentalControlsFeatureAvailable, isArcVmEnabled, isCrostiniSupported, isGuest, isInputDeviceSettingsSplitEnabled, isKerberosEnabled, isPluginVmAvailable, isPowerwashAllowed, isRevampWayfindingEnabled} from './common/load_time_booleans.js';
import * as routesMojom from './mojom-webui/routes.mojom-webui.js';

/**
 * Class for navigable routes. Routes are representing by a tree data structure.
 */
export class Route {
  depth: number;

  /**
   * Whether this route corresponds to a navigable dialog. Navigable dialog
   * routes must belong to a |section|.
   */
  isNavigableDialog: boolean;

  /**
   * The top-level page/section that this route belongs to. Values are derived
   * from the Section enum in routes.mojom.
   */
  section: routesMojom.Section|null;

  /**
   * The document title that should be displayed for this route.
   */
  title: string|undefined;

  /**
   * The parent route, or null if this is a root route.
   */
  parent: Route|null;

  /**
   * The URL path starting with a forward slash. e.g. `/internet`.
   */
  path: string;

  constructor(path: string, title?: string) {
    assert(path.startsWith('/'));

    this.path = path;
    this.title = title;
    this.parent = null;
    this.depth = 0;
    this.isNavigableDialog = false;
    this.section = null;
  }

  /**
   * @returns A new Route instance that is a child of this route. If |path| does
   * not have a leading slash, then it extends this route's path. Else, the
   * given path is set.
   */
  createChild(path: string, title?: string): Route {
    assert(path);

    const childPath = path.startsWith('/') ? path : `${this.path}/${path}`;
    const route = new Route(childPath, title);
    route.parent = this;
    route.section = this.section;
    route.depth = this.depth + 1;
    return route;
  }

  /**
   * Returns the absolute path string for this Route, assuming this function
   * has been called from within chrome://os-settings.
   */
  getAbsolutePath(): string {
    return window.location.origin + this.path;
  }

  /**
   * Returns true if this route matches, or is an ancestor of, the parameter.
   */
  contains(route: Route): boolean {
    for (let curr: Route|null = route; curr !== null; curr = curr.parent) {
      if (this === curr) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns true if this route is a subpage of a section.
   */
  isSubpage(): boolean {
    return !this.isNavigableDialog && !!this.parent && this.section !== null &&
        this.parent.section === this.section;
  }

  /**
   * Returns the top-most ancestor Route for this route's `section`. If this
   * route has no `section` then returns null.
   */
  getSectionAncestor(): Route|null {
    if (this.section === null) {
      return null;
    }

    let curr: Route = this;
    while (curr.parent && curr.parent.section !== null) {
      curr = curr.parent;
    }

    return curr;
  }
}

interface MinimumRoutes {
  BASIC: Route;
  ADVANCED: Route;
  ABOUT: Route;
}

/**
 * Specifies all possible routes in CrOS Settings. Keep routes alphabetized.
 */
export interface OsSettingsRoutes extends MinimumRoutes {
  A11Y_AUDIO_AND_CAPTIONS: Route;
  A11Y_CURSOR_AND_TOUCHPAD: Route;
  A11Y_DISPLAY_AND_MAGNIFICATION: Route;
  A11Y_KEYBOARD_AND_TEXT_INPUT: Route;
  A11Y_TEXT_TO_SPEECH: Route;
  A11Y_CHROMEVOX: Route;
  A11Y_SELECT_TO_SPEAK: Route;
  ABOUT: Route;
  ABOUT_DETAILED_BUILD_INFO: Route;
  ACCOUNTS: Route;
  ACCOUNT_MANAGER: Route;
  ADVANCED: Route;
  APN: Route;
  APP_NOTIFICATIONS: Route;
  APP_NOTIFICATIONS_MANAGER: Route;
  APP_MANAGEMENT: Route;
  APP_MANAGEMENT_DETAIL: Route;
  APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS: Route;
  APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES: Route;
  APP_PARENTAL_CONTROLS: Route;
  APPS: Route;
  ANDROID_APPS_DETAILS: Route;
  ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES: Route;
  AUDIO: Route;
  CROSTINI: Route;
  CROSTINI_ANDROID_ADB: Route;
  CROSTINI_DETAILS: Route;
  CROSTINI_DISK_RESIZE: Route;
  CROSTINI_EXPORT_IMPORT: Route;
  CROSTINI_EXTRA_CONTAINERS: Route;
  CROSTINI_PORT_FORWARDING: Route;
  CROSTINI_SHARED_PATHS: Route;
  CROSTINI_SHARED_USB_DEVICES: Route;
  BASIC: Route;
  BLUETOOTH: Route;
  BLUETOOTH_DEVICES: Route;
  BLUETOOTH_DEVICE_DETAIL: Route;
  BLUETOOTH_SAVED_DEVICES: Route;
  BRUSCHETTA_DETAILS: Route;
  BRUSCHETTA_SHARED_USB_DEVICES: Route;
  BRUSCHETTA_SHARED_PATHS: Route;
  CHANGE_PICTURE: Route;
  CUSTOMIZE_PEN_BUTTONS: Route;
  CUSTOMIZE_TABLET_BUTTONS: Route;
  CUPS_PRINTERS: Route;
  CUSTOMIZE_MOUSE_BUTTONS: Route;
  DARK_MODE: Route;
  DATETIME: Route;
  DATETIME_TIMEZONE_SUBPAGE: Route;
  DEVICE: Route;
  DISPLAY: Route;
  EXTERNAL_STORAGE_PREFERENCES: Route;
  FINGERPRINT: Route;
  FILES: Route;
  GOOGLE_ASSISTANT: Route;
  GOOGLE_DRIVE: Route;
  GRAPHICS_TABLET: Route;
  HOTSPOT_DETAIL: Route;
  INTERNET: Route;
  INTERNET_NETWORKS: Route;
  KERBEROS: Route;
  KERBEROS_ACCOUNTS_V2: Route;
  KEYBOARD: Route;
  KNOWN_NETWORKS: Route;
  LOCK_SCREEN: Route;
  MANAGE_ACCESSIBILITY: Route;
  MANAGE_FACEGAZE_SETTINGS: Route;
  MANAGE_ISOLATED_WEB_APPS: Route;
  MANAGE_SWITCH_ACCESS_SETTINGS: Route;
  MANAGE_TTS_SETTINGS: Route;
  MULTIDEVICE: Route;
  MULTIDEVICE_FEATURES: Route;
  NEARBY_SHARE: Route;
  NETWORK_DETAIL: Route;
  OFFICE: Route;
  ON_STARTUP: Route;
  ONE_DRIVE: Route;
  OS_ACCESSIBILITY: Route;
  OS_LANGUAGES: Route;
  OS_LANGUAGES_APP_LANGUAGES: Route;
  OS_LANGUAGES_EDIT_DICTIONARY: Route;
  OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY: Route;
  OS_LANGUAGES_INPUT: Route;
  OS_LANGUAGES_INPUT_METHOD_OPTIONS: Route;
  OS_LANGUAGES_LANGUAGES: Route;
  OS_PRINTING: Route;
  OS_PRIVACY: Route;
  OS_RESET: Route;
  OS_SEARCH: Route;
  OS_SYNC: Route;
  OS_PEOPLE: Route;
  PASSPOINT_DETAIL: Route;
  PER_DEVICE_KEYBOARD: Route;
  PER_DEVICE_KEYBOARD_REMAP_KEYS: Route;
  PER_DEVICE_MOUSE: Route;
  PER_DEVICE_POINTING_STICK: Route;
  PER_DEVICE_TOUCHPAD: Route;
  PERSONALIZATION: Route;
  POINTERS: Route;
  POWER: Route;
  PRIVACY: Route;
  PRIVACY_HUB: Route;
  PRIVACY_HUB_CAMERA: Route;
  PRIVACY_HUB_GEOLOCATION: Route;
  PRIVACY_HUB_GEOLOCATION_ADVANCED: Route;
  PRIVACY_HUB_MICROPHONE: Route;
  SEARCH: Route;
  SEARCH_SUBPAGE: Route;
  SMART_PRIVACY: Route;
  SMB_SHARES: Route;
  STORAGE: Route;
  STYLUS: Route;
  SYNC: Route;
  SYNC_ADVANCED: Route;
  SYSTEM_PREFERENCES: Route;

  // Internal routes
  INTERNAL_STORYBOOK: Route;
}

function createSection(
    parent: Route|null, path: string, section: routesMojom.Section,
    title?: string): Route {
  let route: Route;
  if (parent) {
    route = parent.createChild(`/${path}`, title);
  } else {
    route = new Route(`/${path}`, title);
  }
  route.section = section;
  return route;
}

function createSubpage(
    parent: Route, path: string, _subpage: routesMojom.Subpage): Route {
  // TODO(khorimoto): Add |subpage| to the Route object.
  return parent.createChild('/' + path);
}

/**
 * Creates Route objects for each path corresponding to CrOS settings content.
 */
export function createRoutes(): OsSettingsRoutes {
  const r: Partial<OsSettingsRoutes> = {};
  const {Section, Subpage} = routesMojom;

  // Special routes:
  // BASIC is the main page which loads if no path is provided. AKA Root page.
  r.BASIC = new Route('/');
  // ADVANCED is a non-navigable route. It only serves as a parent to group
  // child routes under the advanced section and is never allowed direct
  // navigation.
  r.ADVANCED = new Route('/advanced');

  // Network section.
  r.INTERNET = createSection(
      r.BASIC, routesMojom.NETWORK_SECTION_PATH, Section.kNetwork);
  // Note: INTERNET_NETWORKS and NETWORK_DETAIL are special cases because they
  // includes several subpages, one per network type. Default to kWifiNetworks
  // and kWifiDetails subpages.
  r.INTERNET_NETWORKS = createSubpage(
      r.INTERNET, routesMojom.NETWORKS_SUBPAGE_BASE_PATH,
      Subpage.kWifiNetworks);
  r.NETWORK_DETAIL = createSubpage(
      r.INTERNET, routesMojom.WIFI_DETAILS_SUBPAGE_PATH, Subpage.kWifiDetails);
  r.KNOWN_NETWORKS = createSubpage(
      r.INTERNET, routesMojom.KNOWN_NETWORKS_SUBPAGE_PATH,
      Subpage.kKnownNetworks);
  r.HOTSPOT_DETAIL = createSubpage(
      r.INTERNET, routesMojom.HOTSPOT_SUBPAGE_PATH, Subpage.kHotspotDetails);

  if (loadTimeData.getBoolean('isApnRevampEnabled')) {
    r.APN =
        createSubpage(r.INTERNET, routesMojom.APN_SUBPAGE_PATH, Subpage.kApn);
  }
  r.PASSPOINT_DETAIL = createSubpage(
      r.INTERNET, routesMojom.PASSPOINT_DETAIL_SUBPAGE_PATH,
      Subpage.kPasspointDetails);

  // Bluetooth section.
  r.BLUETOOTH = createSection(
      r.BASIC, routesMojom.BLUETOOTH_SECTION_PATH, Section.kBluetooth);
  r.BLUETOOTH_DEVICES = createSubpage(
      r.BLUETOOTH, routesMojom.BLUETOOTH_DEVICES_SUBPAGE_PATH,
      Subpage.kBluetoothDevices);
  r.BLUETOOTH_DEVICE_DETAIL = createSubpage(
      r.BLUETOOTH, routesMojom.BLUETOOTH_DEVICE_DETAIL_SUBPAGE_PATH,
      Subpage.kBluetoothDeviceDetail);
  if (loadTimeData.getBoolean('enableSavedDevicesFlag') &&
      loadTimeData.getBoolean('isCrossDeviceFeatureSuiteEnabled')) {
    r.BLUETOOTH_SAVED_DEVICES = createSubpage(
        r.BLUETOOTH, routesMojom.BLUETOOTH_SAVED_DEVICES_SUBPAGE_PATH,
        Subpage.kBluetoothSavedDevices);
  }

  // MultiDevice section.
  if (!isGuest() &&
      loadTimeData.getBoolean('isCrossDeviceFeatureSuiteEnabled')) {
    r.MULTIDEVICE = createSection(
        r.BASIC, routesMojom.MULTI_DEVICE_SECTION_PATH, Section.kMultiDevice);
    r.MULTIDEVICE_FEATURES = createSubpage(
        r.MULTIDEVICE, routesMojom.MULTI_DEVICE_FEATURES_SUBPAGE_PATH,
        Subpage.kMultiDeviceFeatures);
    if (loadTimeData.getBoolean('isNearbyShareSupported')) {
      r.NEARBY_SHARE = createSubpage(
          r.MULTIDEVICE, routesMojom.NEARBY_SHARE_SUBPAGE_PATH,
          Subpage.kNearbyShare);
    }
  }

  // People section.
  if (!isGuest()) {
    r.OS_PEOPLE = createSection(
        r.BASIC, routesMojom.PEOPLE_SECTION_PATH, Section.kPeople);

    if (!isRevampWayfindingEnabled()) {
      r.ACCOUNT_MANAGER = createSubpage(
          r.OS_PEOPLE, routesMojom.MY_ACCOUNTS_SUBPAGE_PATH,
          Subpage.kMyAccounts);
      // TODO(b/305747266) : Disambiguate the names for OS_SYNC and SYNC.
      r.OS_SYNC = createSubpage(
          r.OS_PEOPLE, routesMojom.SYNC_SUBPAGE_PATH, Subpage.kSync);
      r.SYNC = createSubpage(
          r.OS_PEOPLE, routesMojom.SYNC_SETUP_SUBPAGE_PATH, Subpage.kSyncSetup);
    }
  }

  // Kerberos section.
  if (isKerberosEnabled()) {
    r.KERBEROS = createSection(
        r.BASIC, routesMojom.KERBEROS_SECTION_PATH, Section.kKerberos);
    r.KERBEROS_ACCOUNTS_V2 = createSubpage(
        r.KERBEROS, routesMojom.KERBEROS_ACCOUNTS_V2_SUBPAGE_PATH,
        Subpage.kKerberosAccountsV2);
  }

  // Device section.
  r.DEVICE =
      createSection(r.BASIC, routesMojom.DEVICE_SECTION_PATH, Section.kDevice);
  r.POINTERS = createSubpage(
      r.DEVICE, routesMojom.POINTERS_SUBPAGE_PATH, Subpage.kPointers);
  r.KEYBOARD = createSubpage(
      r.DEVICE, routesMojom.KEYBOARD_SUBPAGE_PATH, Subpage.kKeyboard);
  r.STYLUS =
      createSubpage(r.DEVICE, routesMojom.STYLUS_SUBPAGE_PATH, Subpage.kStylus);
  r.DISPLAY = createSubpage(
      r.DEVICE, routesMojom.DISPLAY_SUBPAGE_PATH, Subpage.kDisplay);
  r.AUDIO =
      createSubpage(r.DEVICE, routesMojom.AUDIO_SUBPAGE_PATH, Subpage.kAudio);
  if (isInputDeviceSettingsSplitEnabled()) {
    r.PER_DEVICE_KEYBOARD = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_KEYBOARD_SUBPAGE_PATH,
        Subpage.kPerDeviceKeyboard);
    r.PER_DEVICE_MOUSE = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_MOUSE_SUBPAGE_PATH,
        Subpage.kPerDeviceMouse);
    r.PER_DEVICE_POINTING_STICK = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_POINTING_STICK_SUBPAGE_PATH,
        Subpage.kPerDevicePointingStick);
    r.PER_DEVICE_TOUCHPAD = createSubpage(
        r.DEVICE, routesMojom.PER_DEVICE_TOUCHPAD_SUBPAGE_PATH,
        Subpage.kPerDeviceTouchpad);
    r.PER_DEVICE_KEYBOARD_REMAP_KEYS = createSubpage(
        r.PER_DEVICE_KEYBOARD,
        routesMojom.PER_DEVICE_KEYBOARD_REMAP_KEYS_SUBPAGE_PATH,
        Subpage.kPerDeviceKeyboardRemapKeys);
  }
  if (loadTimeData.getBoolean('enablePeripheralCustomization')) {
    r.GRAPHICS_TABLET = createSubpage(
        r.DEVICE, routesMojom.GRAPHICS_TABLET_SUBPAGE_PATH,
        Subpage.kGraphicsTablet);
    if (r.PER_DEVICE_MOUSE) {
      r.CUSTOMIZE_MOUSE_BUTTONS = createSubpage(
          r.PER_DEVICE_MOUSE, routesMojom.CUSTOMIZE_MOUSE_BUTTONS_SUBPAGE_PATH,
          Subpage.kCustomizeMouseButtons);
    }
    r.CUSTOMIZE_TABLET_BUTTONS = createSubpage(
        r.GRAPHICS_TABLET, routesMojom.CUSTOMIZE_TABLET_BUTTONS_SUBPAGE_PATH,
        Subpage.kCustomizeTabletButtons);
    r.CUSTOMIZE_PEN_BUTTONS = createSubpage(
        r.GRAPHICS_TABLET, routesMojom.CUSTOMIZE_PEN_BUTTONS_SUBPAGE_PATH,
        Subpage.kCustomizePenButtons);
  }

  // Personalization section.
  r.PERSONALIZATION = createSection(
      r.BASIC, routesMojom.PERSONALIZATION_SECTION_PATH,
      Section.kPersonalization);

  // Apps section.
  r.APPS = createSection(r.BASIC, routesMojom.APPS_SECTION_PATH, Section.kApps);
  r.APP_NOTIFICATIONS = createSubpage(
      r.APPS, routesMojom.APP_NOTIFICATIONS_SUBPAGE_PATH,
      Subpage.kAppNotifications);
  if (isRevampWayfindingEnabled()) {
    r.APP_NOTIFICATIONS_MANAGER = createSubpage(
        r.APP_NOTIFICATIONS, routesMojom.APP_NOTIFICATIONS_MANAGER_SUBPAGE_PATH,
        Subpage.kAppNotificationsManager);
  }
  r.APP_MANAGEMENT = createSubpage(
      r.APPS, routesMojom.APP_MANAGEMENT_SUBPAGE_PATH, Subpage.kAppManagement);
  r.APP_MANAGEMENT_DETAIL = createSubpage(
      r.APP_MANAGEMENT, routesMojom.APP_DETAILS_SUBPAGE_PATH,
      Subpage.kAppDetails);
  if (androidAppsVisible()) {
    r.ANDROID_APPS_DETAILS = createSubpage(
        r.APPS, routesMojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH,
        Subpage.kGooglePlayStore);
    if (isArcVmEnabled()) {
      r.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES = createSubpage(
          r.ANDROID_APPS_DETAILS,
          routesMojom.ARC_VM_USB_PREFERENCES_SUBPAGE_PATH,
          Subpage.kArcVmUsbPreferences);
    }
  }
  if (isPluginVmAvailable()) {
    r.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS = createSubpage(
        r.APP_MANAGEMENT, routesMojom.PLUGIN_VM_SHARED_PATHS_SUBPAGE_PATH,
        Subpage.kPluginVmSharedPaths);
    r.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES = createSubpage(
        r.APP_MANAGEMENT, routesMojom.PLUGIN_VM_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kPluginVmUsbPreferences);
  }
  r.MANAGE_ISOLATED_WEB_APPS = createSubpage(
      r.APPS, routesMojom.MANAGE_ISOLATED_WEB_APPS_SUBPAGE_PATH,
      Subpage.kManageIsolatedWebApps);
  if (isAppParentalControlsFeatureAvailable()) {
    r.APP_PARENTAL_CONTROLS = createSubpage(
        r.APPS, routesMojom.APP_PARENTAL_CONTROLS_SUBPAGE_PATH,
        Subpage.kAppParentalControls);
  }

  // Accessibility section.
  r.OS_ACCESSIBILITY = createSection(
      r.BASIC, routesMojom.ACCESSIBILITY_SECTION_PATH, Section.kAccessibility);
  r.MANAGE_ACCESSIBILITY = createSubpage(
      r.OS_ACCESSIBILITY, routesMojom.MANAGE_ACCESSIBILITY_SUBPAGE_PATH,
      Subpage.kManageAccessibility);
  const a11yParentRoute = loadTimeData.getBoolean('isKioskModeActive') ?
      r.MANAGE_ACCESSIBILITY :
      r.OS_ACCESSIBILITY;
  r.A11Y_TEXT_TO_SPEECH = createSubpage(
      a11yParentRoute, routesMojom.TEXT_TO_SPEECH_PAGE_PATH,
      Subpage.kTextToSpeechPage);
  r.A11Y_DISPLAY_AND_MAGNIFICATION = createSubpage(
      a11yParentRoute, routesMojom.DISPLAY_AND_MAGNIFICATION_SUBPAGE_PATH,
      Subpage.kDisplayAndMagnification);
  r.A11Y_KEYBOARD_AND_TEXT_INPUT = createSubpage(
      a11yParentRoute, routesMojom.KEYBOARD_AND_TEXT_INPUT_SUBPAGE_PATH,
      Subpage.kKeyboardAndTextInput);
  r.A11Y_CURSOR_AND_TOUCHPAD = createSubpage(
      a11yParentRoute, routesMojom.CURSOR_AND_TOUCHPAD_SUBPAGE_PATH,
      Subpage.kCursorAndTouchpad);
  r.A11Y_AUDIO_AND_CAPTIONS = createSubpage(
      a11yParentRoute, routesMojom.AUDIO_AND_CAPTIONS_SUBPAGE_PATH,
      Subpage.kAudioAndCaptions);
  r.A11Y_CHROMEVOX = createSubpage(
      r.A11Y_TEXT_TO_SPEECH, routesMojom.CHROME_VOX_SUBPAGE_PATH,
      Subpage.kChromeVox);
  r.A11Y_SELECT_TO_SPEAK = createSubpage(
      r.A11Y_TEXT_TO_SPEECH, routesMojom.SELECT_TO_SPEAK_SUBPAGE_PATH,
      Subpage.kSelectToSpeak);
  r.MANAGE_TTS_SETTINGS = createSubpage(
      r.A11Y_TEXT_TO_SPEECH, routesMojom.TEXT_TO_SPEECH_SUBPAGE_PATH,
      Subpage.kTextToSpeech);
  r.MANAGE_SWITCH_ACCESS_SETTINGS = createSubpage(
      r.A11Y_KEYBOARD_AND_TEXT_INPUT,
      routesMojom.SWITCH_ACCESS_OPTIONS_SUBPAGE_PATH,
      Subpage.kSwitchAccessOptions);
  r.MANAGE_FACEGAZE_SETTINGS = createSubpage(
      r.A11Y_CURSOR_AND_TOUCHPAD, routesMojom.FACE_GAZE_SETTINGS_SUBPAGE_PATH,
      Subpage.kFaceGazeSettings);

  // Privacy and Security section.
  r.OS_PRIVACY = createSection(
      r.BASIC, routesMojom.PRIVACY_AND_SECURITY_SECTION_PATH,
      Section.kPrivacyAndSecurity);
  r.LOCK_SCREEN = createSubpage(
      r.OS_PRIVACY, routesMojom.SECURITY_AND_SIGN_IN_SUBPAGE_PATH_V2,
      Subpage.kSecurityAndSignInV2);
  r.FINGERPRINT = createSubpage(
      r.LOCK_SCREEN, routesMojom.FINGERPRINT_SUBPAGE_PATH_V2,
      Subpage.kFingerprintV2);
  r.ACCOUNTS = createSubpage(
      r.OS_PRIVACY, routesMojom.MANAGE_OTHER_PEOPLE_SUBPAGE_PATH_V2,
      Subpage.kManageOtherPeopleV2);
  r.SMART_PRIVACY = createSubpage(
      r.OS_PRIVACY, routesMojom.SMART_PRIVACY_SUBPAGE_PATH,
      Subpage.kSmartPrivacy);
  r.PRIVACY_HUB = createSubpage(
      r.OS_PRIVACY, routesMojom.PRIVACY_HUB_SUBPAGE_PATH, Subpage.kPrivacyHub);
  r.PRIVACY_HUB_MICROPHONE = createSubpage(
      r.OS_PRIVACY, routesMojom.PRIVACY_HUB_MICROPHONE_SUBPAGE_PATH,
      Subpage.kPrivacyHubMicrophone);
  r.PRIVACY_HUB_GEOLOCATION = createSubpage(
      r.OS_PRIVACY, routesMojom.PRIVACY_HUB_GEOLOCATION_SUBPAGE_PATH,
      Subpage.kPrivacyHubGeolocation);
  r.PRIVACY_HUB_GEOLOCATION_ADVANCED = createSubpage(
      r.PRIVACY_HUB_GEOLOCATION,
      routesMojom.PRIVACY_HUB_GEOLOCATION_ADVANCED_SUBPAGE_PATH,
      Subpage.kPrivacyHubGeolocationAdvanced);
  r.PRIVACY_HUB_CAMERA = createSubpage(
      r.OS_PRIVACY, routesMojom.PRIVACY_HUB_CAMERA_SUBPAGE_PATH,
      Subpage.kPrivacyHubCamera);

  // About section.
  r.ABOUT = createSection(
      /*parent=*/ null, routesMojom.ABOUT_CHROME_OS_SECTION_PATH,
      Section.kAboutChromeOs);
  r.ABOUT_DETAILED_BUILD_INFO = createSubpage(
      r.ABOUT, routesMojom.DETAILED_BUILD_INFO_SUBPAGE_PATH,
      Subpage.kDetailedBuildInfo);

  // Internal pages (under About section).
  r.INTERNAL_STORYBOOK = createSubpage(
      r.ABOUT, routesMojom.INTERNAL_STORYBOOK_SUBPAGE_PATH,
      Subpage.kInternalStorybook);

  if (isRevampWayfindingEnabled()) {
    // Device section, Input subpages.
    const inputParentRoute = isInputDeviceSettingsSplitEnabled() ?
        r.PER_DEVICE_KEYBOARD :
        r.KEYBOARD;
    assert(inputParentRoute);
    r.OS_LANGUAGES_INPUT = createSubpage(
        inputParentRoute, routesMojom.INPUT_SUBPAGE_PATH, Subpage.kInput);
    r.OS_LANGUAGES_INPUT_METHOD_OPTIONS = createSubpage(
        r.OS_LANGUAGES_INPUT, routesMojom.INPUT_METHOD_OPTIONS_SUBPAGE_PATH,
        Subpage.kInputMethodOptions);
    r.OS_LANGUAGES_EDIT_DICTIONARY = createSubpage(
        r.OS_LANGUAGES_INPUT, routesMojom.EDIT_DICTIONARY_SUBPAGE_PATH,
        Subpage.kEditDictionary);
    r.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY = createSubpage(
        r.OS_LANGUAGES_INPUT,
        routesMojom.JAPANESE_MANAGE_USER_DICTIONARY_SUBPAGE_PATH,
        Subpage.kJapaneseManageUserDictionary);

    // System Preferences section.
    r.SYSTEM_PREFERENCES = createSection(
        r.BASIC, routesMojom.SYSTEM_PREFERENCES_SECTION_PATH,
        Section.kSystemPreferences);

    // Date and Time subpages.
    r.DATETIME_TIMEZONE_SUBPAGE = createSubpage(
        r.SYSTEM_PREFERENCES, routesMojom.TIME_ZONE_SUBPAGE_PATH,
        Subpage.kTimeZone);

    // Files subpages.
    if (!isGuest()) {
      r.GOOGLE_DRIVE = createSubpage(
          r.SYSTEM_PREFERENCES, routesMojom.GOOGLE_DRIVE_SUBPAGE_PATH,
          Subpage.kGoogleDrive);
      if (loadTimeData.getBoolean('showOfficeSettings')) {
        r.OFFICE = createSubpage(
            r.SYSTEM_PREFERENCES, routesMojom.OFFICE_FILES_SUBPAGE_PATH,
            Subpage.kOfficeFiles);
      }
      if (loadTimeData.getBoolean('showOneDriveSettings')) {
        r.ONE_DRIVE = createSubpage(
            r.SYSTEM_PREFERENCES, routesMojom.ONE_DRIVE_SUBPAGE_PATH,
            Subpage.kOneDrive);
      }
      r.SMB_SHARES = createSubpage(
          r.SYSTEM_PREFERENCES, routesMojom.NETWORK_FILE_SHARES_SUBPAGE_PATH,
          Subpage.kNetworkFileShares);
    }

    // Language subpages.
    r.OS_LANGUAGES_LANGUAGES = createSubpage(
        r.SYSTEM_PREFERENCES, routesMojom.LANGUAGES_SUBPAGE_PATH,
        Subpage.kLanguages);
    if (loadTimeData.getBoolean('isPerAppLanguageEnabled')) {
      r.OS_LANGUAGES_APP_LANGUAGES = createSubpage(
          r.OS_LANGUAGES_LANGUAGES, routesMojom.APP_LANGUAGES_SUBPAGE_PATH,
          Subpage.kAppLanguages);
    }

    // Search and Assistant subpages.
    r.SEARCH_SUBPAGE = createSubpage(
        r.SYSTEM_PREFERENCES, routesMojom.SEARCH_SUBPAGE_PATH, Subpage.kSearch);
    r.GOOGLE_ASSISTANT = createSubpage(
        r.SYSTEM_PREFERENCES, routesMojom.ASSISTANT_SUBPAGE_PATH,
        Subpage.kAssistant);

    // Storage and power subpages.
    r.STORAGE = createSubpage(
        r.SYSTEM_PREFERENCES, routesMojom.STORAGE_SUBPAGE_PATH,
        Subpage.kStorage);
    r.EXTERNAL_STORAGE_PREFERENCES = createSubpage(
        r.STORAGE, routesMojom.EXTERNAL_STORAGE_SUBPAGE_PATH,
        Subpage.kExternalStorage);
    r.POWER = createSubpage(
        r.SYSTEM_PREFERENCES, routesMojom.POWER_SUBPAGE_PATH, Subpage.kPower);

    // Printing subpage.
    r.CUPS_PRINTERS = createSubpage(
        r.DEVICE, routesMojom.PRINTING_DETAILS_SUBPAGE_PATH,
        Subpage.kPrintingDetails);

    // Crostini subpages.
    if (isCrostiniSupported()) {
      r.CROSTINI_DETAILS = createSubpage(
          r.ABOUT, routesMojom.CROSTINI_DETAILS_SUBPAGE_PATH,
          Subpage.kCrostiniDetails);

      r.BRUSCHETTA_DETAILS = createSubpage(
          r.ABOUT, routesMojom.BRUSCHETTA_DETAILS_SUBPAGE_PATH,
          Subpage.kBruschettaDetails);
    }

    // Sync subpages.
    if (!isGuest()) {
      assert(r.OS_PRIVACY);
      // TODO(b/305747266) : Disambiguate the names for OS_SYNC and SYNC.
      r.OS_SYNC = createSubpage(
          r.OS_PRIVACY, routesMojom.SYNC_SUBPAGE_PATH, Subpage.kSync);
      r.SYNC = createSubpage(
          r.OS_PRIVACY, routesMojom.SYNC_SETUP_SUBPAGE_PATH,
          Subpage.kSyncSetup);
    }
  } else {
    // Date and Time section.
    r.DATETIME = createSection(
        r.ADVANCED, routesMojom.DATE_AND_TIME_SECTION_PATH,
        Section.kDateAndTime);
    r.DATETIME_TIMEZONE_SUBPAGE = createSubpage(
        r.DATETIME, routesMojom.TIME_ZONE_SUBPAGE_PATH, Subpage.kTimeZone);

    // Device section.
    r.STORAGE = createSubpage(
        r.DEVICE, routesMojom.STORAGE_SUBPAGE_PATH, Subpage.kStorage);
    r.EXTERNAL_STORAGE_PREFERENCES = createSubpage(
        r.STORAGE, routesMojom.EXTERNAL_STORAGE_SUBPAGE_PATH,
        Subpage.kExternalStorage);
    r.POWER =
        createSubpage(r.DEVICE, routesMojom.POWER_SUBPAGE_PATH, Subpage.kPower);

    // Files section.
    if (!isGuest()) {
      r.FILES = createSection(
          r.ADVANCED, routesMojom.FILES_SECTION_PATH, Section.kFiles);
      r.GOOGLE_DRIVE = createSubpage(
          r.FILES, routesMojom.GOOGLE_DRIVE_SUBPAGE_PATH, Subpage.kGoogleDrive);
      if (loadTimeData.getBoolean('showOneDriveSettings')) {
        r.ONE_DRIVE = createSubpage(
            r.FILES, routesMojom.ONE_DRIVE_SUBPAGE_PATH, Subpage.kOneDrive);
      }
      if (loadTimeData.getBoolean('showOfficeSettings')) {
        r.OFFICE = createSubpage(
            r.FILES, routesMojom.OFFICE_FILES_SUBPAGE_PATH,
            Subpage.kOfficeFiles);
      }
      r.SMB_SHARES = createSubpage(
          r.FILES, routesMojom.NETWORK_FILE_SHARES_SUBPAGE_PATH,
          Subpage.kNetworkFileShares);
    }

    // Languages and Input section.
    r.OS_LANGUAGES = createSection(
        r.ADVANCED, routesMojom.LANGUAGES_AND_INPUT_SECTION_PATH,
        Section.kLanguagesAndInput);
    r.OS_LANGUAGES_LANGUAGES = createSubpage(
        r.OS_LANGUAGES, routesMojom.LANGUAGES_SUBPAGE_PATH, Subpage.kLanguages);
    r.OS_LANGUAGES_INPUT = createSubpage(
        r.OS_LANGUAGES, routesMojom.INPUT_SUBPAGE_PATH, Subpage.kInput);
    r.OS_LANGUAGES_INPUT_METHOD_OPTIONS = createSubpage(
        r.OS_LANGUAGES_INPUT, routesMojom.INPUT_METHOD_OPTIONS_SUBPAGE_PATH,
        Subpage.kInputMethodOptions);
    r.OS_LANGUAGES_EDIT_DICTIONARY = createSubpage(
        r.OS_LANGUAGES_INPUT, routesMojom.EDIT_DICTIONARY_SUBPAGE_PATH,
        Subpage.kEditDictionary);
    r.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY = createSubpage(
        r.OS_LANGUAGES_INPUT,
        routesMojom.JAPANESE_MANAGE_USER_DICTIONARY_SUBPAGE_PATH,
        Subpage.kJapaneseManageUserDictionary);
    if (loadTimeData.getBoolean('isPerAppLanguageEnabled')) {
      r.OS_LANGUAGES_APP_LANGUAGES = createSubpage(
          r.OS_LANGUAGES_LANGUAGES, routesMojom.APP_LANGUAGES_SUBPAGE_PATH,
          Subpage.kAppLanguages);
    }

    // Reset section.
    if (isPowerwashAllowed()) {
      r.OS_RESET = createSection(
          r.ADVANCED, routesMojom.RESET_SECTION_PATH, Section.kReset);
    }

    // Search and Assistant section.
    r.OS_SEARCH = createSection(
        r.BASIC, routesMojom.SEARCH_AND_ASSISTANT_SECTION_PATH,
        Section.kSearchAndAssistant);
    r.SEARCH_SUBPAGE = createSubpage(
        r.OS_SEARCH, routesMojom.SEARCH_SUBPAGE_PATH, Subpage.kSearch);
    r.GOOGLE_ASSISTANT = createSubpage(
        r.OS_SEARCH, routesMojom.ASSISTANT_SUBPAGE_PATH, Subpage.kAssistant);

    // Printing section.
    r.OS_PRINTING = createSection(
        r.ADVANCED, routesMojom.PRINTING_SECTION_PATH, Section.kPrinting);
    r.CUPS_PRINTERS = createSubpage(
        r.OS_PRINTING, routesMojom.PRINTING_DETAILS_SUBPAGE_PATH,
        Subpage.kPrintingDetails);

    // Crostini section.
    r.CROSTINI = createSection(
        r.ADVANCED, routesMojom.CROSTINI_SECTION_PATH, Section.kCrostini);
    if (isCrostiniSupported()) {
      r.CROSTINI_DETAILS = createSubpage(
          r.CROSTINI, routesMojom.CROSTINI_DETAILS_SUBPAGE_PATH,
          Subpage.kCrostiniDetails);
      r.BRUSCHETTA_DETAILS = createSubpage(
          r.CROSTINI, routesMojom.BRUSCHETTA_DETAILS_SUBPAGE_PATH,
          Subpage.kBruschettaDetails);
    }
  }

  // Crostini details subpages.
  if (isCrostiniSupported()) {
    assert(r.CROSTINI_DETAILS);
    assert(r.BRUSCHETTA_DETAILS);
    r.CROSTINI_SHARED_PATHS = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojom.CROSTINI_MANAGE_SHARED_FOLDERS_SUBPAGE_PATH,
        Subpage.kCrostiniManageSharedFolders);
    r.CROSTINI_SHARED_USB_DEVICES = createSubpage(
        r.CROSTINI_DETAILS, routesMojom.CROSTINI_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kCrostiniUsbPreferences);
    if (loadTimeData.valueExists('showCrostiniExportImport') &&
        loadTimeData.getBoolean('showCrostiniExportImport')) {
      r.CROSTINI_EXPORT_IMPORT = createSubpage(
          r.CROSTINI_DETAILS,
          routesMojom.CROSTINI_BACKUP_AND_RESTORE_SUBPAGE_PATH,
          Subpage.kCrostiniBackupAndRestore);
    }
    if (loadTimeData.valueExists('showCrostiniExtraContainers') &&
        loadTimeData.getBoolean('showCrostiniExtraContainers')) {
      r.CROSTINI_EXTRA_CONTAINERS = createSubpage(
          r.CROSTINI_DETAILS,
          routesMojom.CROSTINI_EXTRA_CONTAINERS_SUBPAGE_PATH,
          Subpage.kCrostiniExtraContainers);
    }

    r.CROSTINI_ANDROID_ADB = createSubpage(
        r.CROSTINI_DETAILS,
        routesMojom.CROSTINI_DEVELOP_ANDROID_APPS_SUBPAGE_PATH,
        Subpage.kCrostiniDevelopAndroidApps);
    r.CROSTINI_PORT_FORWARDING = createSubpage(
        r.CROSTINI_DETAILS, routesMojom.CROSTINI_PORT_FORWARDING_SUBPAGE_PATH,
        Subpage.kCrostiniPortForwarding);

    r.BRUSCHETTA_SHARED_USB_DEVICES = createSubpage(
        r.BRUSCHETTA_DETAILS,
        routesMojom.BRUSCHETTA_USB_PREFERENCES_SUBPAGE_PATH,
        Subpage.kBruschettaUsbPreferences);
    r.BRUSCHETTA_SHARED_PATHS = createSubpage(
        r.BRUSCHETTA_DETAILS,
        routesMojom.BRUSCHETTA_MANAGE_SHARED_FOLDERS_SUBPAGE_PATH,
        Subpage.kBruschettaManageSharedFolders);
  }

  return r as OsSettingsRoutes;
}

const PATH_REDIRECT_PAIRS: Array<[string, string]> = [
  [
    routesMojom.MY_ACCOUNTS_SUBPAGE_PATH,
    routesMojom.PEOPLE_SECTION_PATH,
  ],
  [
    routesMojom.DATE_AND_TIME_SECTION_PATH,
    routesMojom.SYSTEM_PREFERENCES_SECTION_PATH,
  ],
  [
    routesMojom.FILES_SECTION_PATH,
    routesMojom.SYSTEM_PREFERENCES_SECTION_PATH,
  ],
  // TODO(b/309808834) Remove this pair once the Bluetooth L1 page is revamped
  // with up-leveled content.
  [
    routesMojom.BLUETOOTH_SECTION_PATH,
    routesMojom.BLUETOOTH_DEVICES_SUBPAGE_PATH,
  ],
];

/**
 * An object of path redirects. The key represents a given path and the value
 * represents the resulting path that should be redirected to. Path strings
 * always include a leading slash.
 */
export const PATH_REDIRECTS =
    Object.fromEntries(PATH_REDIRECT_PAIRS.map(([path, redirectPath]) => {
      return [`/${path}`, `/${redirectPath}`];
    }));
