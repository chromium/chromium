// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import '../constants/routes.mojom-lite.js';

// #import {OsSettingsRoutes} from './os_settings_routes.m.js';
// #import {Route, Router} from '../router.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

cr.define('settings', function() {
  /**
   * @param {!settings.Route} parent
   * @param {string} path
   * @param {!chromeos.settings.mojom.Section} section
   * @return {!settings.Route}
   */
  function createSection(parent, path, section) {
    // TODO(khorimoto): Add |section| to the the Route object.
    return parent.createSection('/' + path, /*section=*/ path);
  }

  /**
   * @param {!settings.Route} parent
   * @param {string} path
   * @param {!chromeos.settings.mojom.Subpage} subpage
   * @return {!settings.Route}
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
    const mojom = chromeos.settings.mojom;
    const Section = mojom.Section;
    const Subpage = mojom.Subpage;

    const r = /** @type {!OsSettingsRoutes} */ ({});

    // Special routes: BASIC is the main page which loads if no path is
    // provided, ADVANCED is the bottom section of the main page which is not
    // visible unless the user enables it, and OS_SIGN_OUT is a sign out dialog.
    r.BASIC = new settings.Route('/');
    r.ADVANCED = new settings.Route('/advanced');
    r.OS_SIGN_OUT = r.BASIC.createChild('/osSignOut');
    r.OS_SIGN_OUT.isNavigableDialog = true;

    // Network section.
    r.INTERNET =
        createSection(r.BASIC, mojom.NETWORK_SECTION_PATH, Section.kNetwork);
    // Note: INTERNET_NETWORKS and NETWORK_DETAIL are special cases because they
    // includes several subpages, one per network type. Default to kWifiNetworks
    // and kWifiDetails subpages.
    r.INTERNET_NETWORKS =
        createSubpage(r.INTERNET, 'networks', Subpage.kWifiNetworks);
    r.NETWORK_DETAIL =
        createSubpage(r.INTERNET, 'networkDetail', Subpage.kWifiDetails);
    r.KNOWN_NETWORKS = createSubpage(
        r.INTERNET, mojom.KNOWN_NETWORKS_SUBPAGE_PATH, Subpage.kKnownNetworks);

    // Bluetooth section.
    r.BLUETOOTH = createSection(
        r.BASIC, mojom.BLUETOOTH_SECTION_PATH, Section.kBluetooth);
    r.BLUETOOTH_DEVICES = createSubpage(
        r.BLUETOOTH, mojom.BLUETOOTH_DEVICES_SUBPAGE_PATH,
        Subpage.kBluetoothDevices);

    // MultiDevice section.
    if (!loadTimeData.getBoolean('isGuest')) {
      r.MULTIDEVICE = createSection(
          r.BASIC, mojom.MULTI_DEVICE_SECTION_PATH, Section.kMultiDevice);
      r.MULTIDEVICE_FEATURES = createSubpage(
          r.MULTIDEVICE, mojom.MULTI_DEVICE_FEATURES_SUBPAGE_PATH,
          Subpage.kMultiDeviceFeatures);
      r.SMART_LOCK = createSubpage(
          r.MULTIDEVICE_FEATURES, mojom.SMART_LOCK_SUBPAGE_PATH,
          Subpage.kSmartLock);
      if (loadTimeData.getBoolean('isNearbyShareSupported')) {
        r.NEARBY_SHARE = createSubpage(
            r.MULTIDEVICE, mojom.NEARBY_SHARE_SUBPAGE_PATH,
            Subpage.kNearbyShare);
      }
    }

    // People section.
    if (!loadTimeData.getBoolean('isGuest')) {
      r.OS_PEOPLE =
          createSection(r.BASIC, mojom.PEOPLE_SECTION_PATH, Section.kPeople);
      r.ACCOUNT_MANAGER = createSubpage(
          r.OS_PEOPLE, mojom.MY_ACCOUNTS_SUBPAGE_PATH, Subpage.kMyAccounts);
      if (loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
        r.OS_SYNC =
            createSubpage(r.OS_PEOPLE, mojom.SYNC_SUBPAGE_PATH, Subpage.kSync);
      }
      r.SYNC = createSubpage(
          r.OS_PEOPLE, mojom.SYNC_DEPRECATED_SUBPAGE_PATH,
          Subpage.kSyncDeprecated);
      if (!loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
        r.SYNC_ADVANCED = createSubpage(
            r.SYNC, mojom.SYNC_DEPRECATED_ADVANCED_SUBPAGE_PATH,
            Subpage.kSyncDeprecatedAdvanced);
      }
      if (!loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled')) {
        r.LOCK_SCREEN = createSubpage(
            r.OS_PEOPLE, mojom.SECURITY_AND_SIGN_IN_SUBPAGE_PATH,
            Subpage.kSecurityAndSignIn);
        r.FINGERPRINT = createSubpage(
            r.LOCK_SCREEN, mojom.FINGERPRINT_SUBPAGE_PATH,
            Subpage.kFingerprint);
        r.ACCOUNTS = createSubpage(
            r.OS_PEOPLE, mojom.MANAGE_OTHER_PEOPLE_SUBPAGE_PATH,
            Subpage.kManageOtherPeople);
      }
      r.KERBEROS_ACCOUNTS = createSubpage(
          r.OS_PEOPLE, mojom.KERBEROS_ACCOUNTS_SUBPAGE_PATH,
          Subpage.kKerberosAccounts);
    }

    const isKerberosEnabled = loadTimeData.valueExists('isKerberosEnabled') &&
        loadTimeData.getBoolean('isKerberosEnabled');
    const isKerberosSettingsSectionEnabled =
        loadTimeData.valueExists('isKerberosSettingsSectionEnabled') &&
        loadTimeData.getBoolean('isKerberosSettingsSectionEnabled');

    // Kerberos section.
    if (isKerberosEnabled && isKerberosSettingsSectionEnabled) {
      r.KERBEROS = createSection(
          r.BASIC, mojom.KERBEROS_SECTION_PATH, Section.kKerberos);
      r.KERBEROS_ACCOUNTS_V2 = createSubpage(
          r.KERBEROS, mojom.KERBEROS_ACCOUNTS_V2_SUBPAGE_PATH,
          Subpage.kKerberosAccountsV2);
    }

    // Device section.
    r.DEVICE =
        createSection(r.BASIC, mojom.DEVICE_SECTION_PATH, Section.kDevice);
    r.POINTERS =
        createSubpage(r.DEVICE, mojom.POINTERS_SUBPAGE_PATH, Subpage.kPointers);
    r.KEYBOARD =
        createSubpage(r.DEVICE, mojom.KEYBOARD_SUBPAGE_PATH, Subpage.kKeyboard);
    r.STYLUS =
        createSubpage(r.DEVICE, mojom.STYLUS_SUBPAGE_PATH, Subpage.kStylus);
    r.DISPLAY =
        createSubpage(r.DEVICE, mojom.DISPLAY_SUBPAGE_PATH, Subpage.kDisplay);
    r.STORAGE =
        createSubpage(r.DEVICE, mojom.STORAGE_SUBPAGE_PATH, Subpage.kStorage);
    r.EXTERNAL_STORAGE_PREFERENCES = createSubpage(
        r.STORAGE, mojom.EXTERNAL_STORAGE_SUBPAGE_PATH,
        Subpage.kExternalStorage);
    r.POWER = createSubpage(r.DEVICE, mojom.POWER_SUBPAGE_PATH, Subpage.kPower);

    // Personalization section.
    if (!loadTimeData.getBoolean('isGuest')) {
      r.PERSONALIZATION = createSection(
          r.BASIC, mojom.PERSONALIZATION_SECTION_PATH,
          Section.kPersonalization);
      r.CHANGE_PICTURE = createSubpage(
          r.PERSONALIZATION, mojom.CHANGE_PICTURE_SUBPAGE_PATH,
          Subpage.kChangePicture);
      r.AMBIENT_MODE = createSubpage(
          r.PERSONALIZATION, mojom.AMBIENT_MODE_SUBPAGE_PATH,
          Subpage.kAmbientMode);
      // Note: AMBIENT_MODE_PHOTOS is a special case because it includes several
      // subpages, one per topic source. Default to
      // kAmbientModeGooglePhotosAlbum subpage.
      r.AMBIENT_MODE_PHOTOS = createSubpage(
          r.AMBIENT_MODE, 'ambientMode/photos',
          Subpage.kAmbientModeGooglePhotosAlbum);
    }

    // Search and Assistant section.
    r.OS_SEARCH = createSection(
        r.BASIC, mojom.SEARCH_AND_ASSISTANT_SECTION_PATH,
        Section.kSearchAndAssistant);
    r.GOOGLE_ASSISTANT = createSubpage(
        r.OS_SEARCH, mojom.ASSISTANT_SUBPAGE_PATH, Subpage.kAssistant);

    // Apps section.
    r.APPS = createSection(r.BASIC, mojom.APPS_SECTION_PATH, Section.kApps);
    r.APP_MANAGEMENT = createSubpage(
        r.APPS, mojom.APP_MANAGEMENT_SUBPAGE_PATH, Subpage.kAppManagement);
    r.APP_MANAGEMENT_DETAIL = createSubpage(
        r.APP_MANAGEMENT, mojom.APP_DETAILS_SUBPAGE_PATH, Subpage.kAppDetails);
    if (loadTimeData.valueExists('androidAppsVisible') &&
        loadTimeData.getBoolean('androidAppsVisible')) {
      r.ANDROID_APPS_DETAILS = createSubpage(
          r.APPS, mojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH,
          Subpage.kGooglePlayStore);
    }
    if (loadTimeData.valueExists('showPluginVm') &&
        loadTimeData.getBoolean('showPluginVm')) {
      r.APP_MANAGEMENT_PLUGIN_VM_SHARED_PATHS = createSubpage(
          r.APP_MANAGEMENT, mojom.PLUGIN_VM_SHARED_PATHS_SUBPAGE_PATH,
          Subpage.kPluginVmSharedPaths);
      r.APP_MANAGEMENT_PLUGIN_VM_SHARED_USB_DEVICES = createSubpage(
          r.APP_MANAGEMENT, mojom.PLUGIN_VM_USB_PREFERENCES_SUBPAGE_PATH,
          Subpage.kPluginVmUsbPreferences);
    }

    // Crostini section.
    if (loadTimeData.valueExists('showCrostini') &&
        loadTimeData.getBoolean('showCrostini')) {
      r.CROSTINI = createSection(
          r.ADVANCED, mojom.CROSTINI_SECTION_PATH, Section.kCrostini);
      r.CROSTINI_DETAILS = createSubpage(
          r.CROSTINI, mojom.CROSTINI_DETAILS_SUBPAGE_PATH,
          Subpage.kCrostiniDetails);
      r.CROSTINI_SHARED_PATHS = createSubpage(
          r.CROSTINI_DETAILS, mojom.CROSTINI_MANAGE_SHARED_FOLDERS_SUBPAGE_PATH,
          Subpage.kCrostiniManageSharedFolders);
      r.CROSTINI_SHARED_USB_DEVICES = createSubpage(
          r.CROSTINI_DETAILS, mojom.CROSTINI_USB_PREFERENCES_SUBPAGE_PATH,
          Subpage.kCrostiniUsbPreferences);
      if (loadTimeData.valueExists('showCrostiniExportImport') &&
          loadTimeData.getBoolean('showCrostiniExportImport')) {
        r.CROSTINI_EXPORT_IMPORT = createSubpage(
            r.CROSTINI_DETAILS, mojom.CROSTINI_BACKUP_AND_RESTORE_SUBPAGE_PATH,
            Subpage.kCrostiniBackupAndRestore);
      }
      r.CROSTINI_ANDROID_ADB = createSubpage(
          r.CROSTINI_DETAILS, mojom.CROSTINI_DEVELOP_ANDROID_APPS_SUBPAGE_PATH,
          Subpage.kCrostiniDevelopAndroidApps);
      r.CROSTINI_PORT_FORWARDING = createSubpage(
          r.CROSTINI_DETAILS, mojom.CROSTINI_PORT_FORWARDING_SUBPAGE_PATH,
          Subpage.kCrostiniPortForwarding);
    }

    // On Startup section.
    r.ON_STARTUP = createSection(
        r.BASIC, mojom.ON_STARTUP_SECTION_PATH, Section.kOnStartup);

    // Date and Time section.
    r.DATETIME = createSection(
        r.ADVANCED, mojom.DATE_AND_TIME_SECTION_PATH, Section.kDateAndTime);
    r.DATETIME_TIMEZONE_SUBPAGE = createSubpage(
        r.DATETIME, mojom.TIME_ZONE_SUBPAGE_PATH, Subpage.kTimeZone);

    // Privacy and Security section.

    if (loadTimeData.getBoolean('isAccountManagementFlowsV2Enabled')) {
      r.OS_PRIVACY = createSection(
          r.BASIC, mojom.PRIVACY_AND_SECURITY_SECTION_PATH,
          Section.kPrivacyAndSecurity);
      r.LOCK_SCREEN = createSubpage(
          r.OS_PRIVACY, mojom.SECURITY_AND_SIGN_IN_SUBPAGE_PATH_V2,
          Subpage.kSecurityAndSignInV2);
      r.FINGERPRINT = createSubpage(
          r.LOCK_SCREEN, mojom.FINGERPRINT_SUBPAGE_PATH_V2,
          Subpage.kFingerprintV2);
      r.ACCOUNTS = createSubpage(
          r.OS_PRIVACY, mojom.MANAGE_OTHER_PEOPLE_SUBPAGE_PATH_V2,
          Subpage.kManageOtherPeopleV2);
    } else {
      r.OS_PRIVACY = createSection(
          r.ADVANCED, mojom.PRIVACY_AND_SECURITY_SECTION_PATH,
          Section.kPrivacyAndSecurity);
    }

    // Languages and Input section.
    r.OS_LANGUAGES = createSection(
        r.ADVANCED, mojom.LANGUAGES_AND_INPUT_SECTION_PATH,
        Section.kLanguagesAndInput);
    if (loadTimeData.getBoolean('enableLanguageSettingsV2')) {
      r.OS_LANGUAGES_LANGUAGES = createSubpage(
          r.OS_LANGUAGES, mojom.LANGUAGES_SUBPAGE_PATH, Subpage.kLanguages);
      r.OS_LANGUAGES_INPUT = createSubpage(
          r.OS_LANGUAGES, mojom.INPUT_SUBPAGE_PATH, Subpage.kInput);
      r.OS_LANGUAGES_INPUT_METHOD_OPTIONS = createSubpage(
          r.OS_LANGUAGES_INPUT, mojom.INPUT_METHOD_OPTIONS_SUBPAGE_PATH,
          Subpage.kInputMethodOptions);
      r.OS_LANGUAGES_EDIT_DICTIONARY = createSubpage(
          r.OS_LANGUAGES_INPUT, mojom.EDIT_DICTIONARY_SUBPAGE_PATH,
          Subpage.kEditDictionary);
    } else {
      r.OS_LANGUAGES_DETAILS = createSubpage(
          r.OS_LANGUAGES, mojom.LANGUAGES_AND_INPUT_DETAILS_SUBPAGE_PATH,
          Subpage.kLanguagesAndInputDetails);
      r.OS_LANGUAGES_INPUT_METHODS = createSubpage(
          r.OS_LANGUAGES_DETAILS, mojom.MANAGE_INPUT_METHODS_SUBPAGE_PATH,
          Subpage.kManageInputMethods);
      r.OS_LANGUAGES_INPUT_METHOD_OPTIONS = createSubpage(
          r.OS_LANGUAGES_DETAILS, mojom.INPUT_METHOD_OPTIONS_SUBPAGE_PATH,
          Subpage.kInputMethodOptions);
    }
    r.OS_LANGUAGES_SMART_INPUTS = createSubpage(
        r.OS_LANGUAGES, mojom.SMART_INPUTS_SUBPAGE_PATH, Subpage.kSmartInputs);


    // Files section.
    if (!loadTimeData.getBoolean('isGuest')) {
      r.FILES =
          createSection(r.ADVANCED, mojom.FILES_SECTION_PATH, Section.kFiles);
      r.SMB_SHARES = createSubpage(
          r.FILES, mojom.NETWORK_FILE_SHARES_SUBPAGE_PATH,
          Subpage.kNetworkFileShares);
    }

    // Printing section.
    r.OS_PRINTING = createSection(
        r.ADVANCED, mojom.PRINTING_SECTION_PATH, Section.kPrinting);
    r.CUPS_PRINTERS = createSubpage(
        r.OS_PRINTING, mojom.PRINTING_DETAILS_SUBPAGE_PATH,
        Subpage.kPrintingDetails);

    // Accessibility section.
    r.OS_ACCESSIBILITY = createSection(
        r.ADVANCED, mojom.ACCESSIBILITY_SECTION_PATH, Section.kAccessibility);
    r.MANAGE_ACCESSIBILITY = createSubpage(
        r.OS_ACCESSIBILITY, mojom.MANAGE_ACCESSIBILITY_SUBPAGE_PATH,
        Subpage.kManageAccessibility);
    r.MANAGE_TTS_SETTINGS = createSubpage(
        r.MANAGE_ACCESSIBILITY, mojom.TEXT_TO_SPEECH_SUBPAGE_PATH,
        Subpage.kTextToSpeech);
    r.MANAGE_SWITCH_ACCESS_SETTINGS = createSubpage(
        r.MANAGE_ACCESSIBILITY, mojom.SWITCH_ACCESS_OPTIONS_SUBPAGE_PATH,
        Subpage.kSwitchAccessOptions);
    r.MANAGE_CAPTION_SETTINGS = createSubpage(
        r.MANAGE_ACCESSIBILITY, mojom.CAPTIONS_SUBPAGE_PATH, Subpage.kCaptions);

    // Reset section.
    if (loadTimeData.valueExists('allowPowerwash') &&
        loadTimeData.getBoolean('allowPowerwash')) {
      r.OS_RESET =
          createSection(r.ADVANCED, mojom.RESET_SECTION_PATH, Section.kReset);
    }

    // About section. Note that this section is a special case, since it is not
    // part of the main page. In this case, the "About Chrome OS" subpage is
    // implemented using createSection().
    // TODO(khorimoto): Add Section.kAboutChromeOs to settings.Route object.
    r.ABOUT = new settings.Route('/' + mojom.ABOUT_CHROME_OS_SECTION_PATH);
    r.ABOUT_ABOUT = r.ABOUT.createSection(
        '/' + mojom.ABOUT_CHROME_OS_DETAILS_SUBPAGE_PATH, 'about');
    r.DETAILED_BUILD_INFO = createSubpage(
        r.ABOUT_ABOUT, mojom.DETAILED_BUILD_INFO_SUBPAGE_PATH,
        Subpage.kDetailedBuildInfo);

    return r;
  }

  /**
   * @return {!settings.Router} A router with at least those routes common to OS
   *     and browser settings. If the window is not in OS settings (based on
   *     loadTimeData) then browser specific routes are added. If the window is
   *     OS settings or if Chrome OS is using a consolidated settings page for
   *     OS and browser settings then OS specific routes are added.
   */
  function buildRouter() {
    return new settings.Router(createOSSettingsRoutes());
  }

  settings.Router.setInstance(buildRouter());

  window.addEventListener('popstate', function(event) {
    // On pop state, do not push the state onto the window.history again.
    const routerInstance = settings.Router.getInstance();
    routerInstance.setCurrentRoute(
        routerInstance.getRouteForPath(window.location.pathname) ||
            routerInstance.getRoutes().BASIC,
        new URLSearchParams(window.location.search), true);
  });

  // TODO(dpapad): Change to 'get routes() {}' in export when we fix a bug in
  // ChromePass that limits the syntax of what can be returned from cr.define().
  /* #export */ const routes = /** @type {!OsSettingsRoutes} */ (
      settings.Router.getInstance().getRoutes());

  // #cr_define_end
  return {
    buildRouterForTesting: buildRouter,
    routes: routes,
  };
});
