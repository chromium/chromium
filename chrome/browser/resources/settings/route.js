// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Specifies all possible routes in settings.
 *
 * @typedef {{
 *   ABOUT: (undefined|!settings.Route),
 *   ABOUT_ABOUT: (undefined|!settings.Route),
 *   ACCESSIBILITY: (undefined|!settings.Route),
 *   ACCOUNTS: (undefined|!settings.Route),
 *   ACCOUNT_MANAGER: (undefined|!settings.Route),
 *   ADVANCED: (undefined|!settings.Route),
 *   ADDRESSES: (undefined|!settings.Route),
 *   APP_MANAGEMENT: (undefined|!settings.Route),
 *   APP_MANAGEMENT_DETAIL: (undefined|!settings.Route),
 *   APPS: (undefined|!settings.Route),
 *   ANDROID_APPS: (undefined|!settings.Route),
 *   ANDROID_APPS_DETAILS: (undefined|!settings.Route),
 *   CROSTINI: (undefined|!settings.Route),
 *   CROSTINI_ANDROID_ADB: (undefined|!settings.Route),
 *   CROSTINI_DETAILS: (undefined|!settings.Route),
 *   CROSTINI_EXPORT_IMPORT: (undefined|!settings.Route),
 *   CROSTINI_SHARED_PATHS: (undefined|!settings.Route),
 *   CROSTINI_SHARED_USB_DEVICES: (undefined|!settings.Route),
 *   APPEARANCE: (undefined|!settings.Route),
 *   AUTOFILL: (undefined|!settings.Route),
 *   BASIC: (undefined|!settings.Route),
 *   BLUETOOTH: (undefined|!settings.Route),
 *   BLUETOOTH_DEVICES: (undefined|!settings.Route),
 *   CAPTIONS: (undefined|!settings.Route),
 *   CERTIFICATES: (undefined|!settings.Route),
 *   CHANGE_PICTURE: (undefined|!settings.Route),
 *   CHROME_CLEANUP: (undefined|!settings.Route),
 *   CLEAR_BROWSER_DATA: (undefined|!settings.Route),
 *   CLOUD_PRINTERS: (undefined|!settings.Route),
 *   CUPS_PRINTERS: (undefined|!settings.Route),
 *   DATETIME: (undefined|!settings.Route),
 *   DATETIME_TIMEZONE_SUBPAGE: (undefined|!settings.Route),
 *   DEFAULT_BROWSER: (undefined|!settings.Route),
 *   DETAILED_BUILD_INFO: (undefined|!settings.Route),
 *   DEVICE: (undefined|!settings.Route),
 *   DISPLAY: (undefined|!settings.Route),
 *   DOWNLOADS: (undefined|!settings.Route),
 *   EDIT_DICTIONARY: (undefined|!settings.Route),
 *   EXTERNAL_STORAGE_PREFERENCES: (undefined|!settings.Route),
 *   FINGERPRINT: (undefined|!settings.Route),
 *   FILES: (undefined|!settings.Route),
 *   FONTS: (undefined|!settings.Route),
 *   GOOGLE_ASSISTANT: (undefined|!settings.Route),
 *   IMPORT_DATA: (undefined|!settings.Route),
 *   INCOMPATIBLE_APPLICATIONS: (undefined|!settings.Route),
 *   INPUT_METHODS: (undefined|!settings.Route),
 *   INTERNET: (undefined|!settings.Route),
 *   INTERNET_NETWORKS: (undefined|!settings.Route),
 *   KERBEROS_ACCOUNTS: (undefined|!settings.Route),
 *   KEYBOARD: (undefined|!settings.Route),
 *   KNOWN_NETWORKS: (undefined|!settings.Route),
 *   LANGUAGES: (undefined|!settings.Route),
 *   LANGUAGES_DETAILS: (undefined|!settings.Route),
 *   LOCK_SCREEN: (undefined|!settings.Route),
 *   MANAGE_ACCESSIBILITY: (undefined|!settings.Route),
 *   MANAGE_CAPTION_SETTINGS: (undefined|!settings.Route),
 *   MANAGE_PROFILE: (undefined|!settings.Route),
 *   MANAGE_SWITCH_ACCESS_SETTINGS: (undefined|!settings.Route),
 *   MANAGE_TTS_SETTINGS: (undefined|!settings.Route),
 *   MULTIDEVICE: (undefined|!settings.Route),
 *   MULTIDEVICE_FEATURES: (undefined|!settings.Route),
 *   NETWORK_DETAIL: (undefined|!settings.Route),
 *   ON_STARTUP: (undefined|!settings.Route),
 *   OS_SYNC: (undefined|!settings.Route),
 *   PASSWORDS: (undefined|!settings.Route),
 *   PAYMENTS: (undefined|!settings.Route),
 *   PEOPLE: (undefined|!settings.Route),
 *   PERSONALIZATION: (undefined|!settings.Route),
 *   PLUGIN_VM: (undefined|!settings.Route),
 *   PLUGIN_VM_DETAILS: (undefined|!settings.Route),
 *   PLUGIN_VM_SHARED_PATHS: (undefined|!settings.Route),
 *   POINTERS: (undefined|!settings.Route),
 *   POWER: (undefined|!settings.Route),
 *   PRINTING: (undefined|!settings.Route),
 *   PRIVACY: (undefined|!settings.Route),
 *   RESET: (undefined|!settings.Route),
 *   RESET_DIALOG: (undefined|!settings.Route),
 *   SEARCH: (undefined|!settings.Route),
 *   SEARCH_ENGINES: (undefined|!settings.Route),
 *   SECURITY_KEYS: (undefined|!settings.Route),
 *   SIGN_OUT: (undefined|!settings.Route),
 *   SITE_SETTINGS: (undefined|!settings.Route),
 *   SITE_SETTINGS_ADS: (undefined|!settings.Route),
 *   SITE_SETTINGS_ALL: (undefined|!settings.Route),
 *   SITE_SETTINGS_AUTOMATIC_DOWNLOADS: (undefined|!settings.Route),
 *   SITE_SETTINGS_BACKGROUND_SYNC: (undefined|!settings.Route),
 *   SITE_SETTINGS_BLUETOOTH_SCANNING: (undefined|!settings.Route),
 *   SITE_SETTINGS_CAMERA: (undefined|!settings.Route),
 *   SITE_SETTINGS_CLIPBOARD: (undefined|!settings.Route),
 *   SITE_SETTINGS_COOKIES: (undefined|!settings.Route),
 *   SITE_SETTINGS_DATA_DETAILS: (undefined|!settings.Route),
 *   SITE_SETTINGS_FLASH: (undefined|!settings.Route),
 *   SITE_SETTINGS_HANDLERS: (undefined|!settings.Route),
 *   SITE_SETTINGS_IMAGES: (undefined|!settings.Route),
 *   SITE_SETTINGS_MIXEDSCRIPT: (undefined|!settings.Route),
 *   SITE_SETTINGS_JAVASCRIPT: (undefined|!settings.Route),
 *   SITE_SETTINGS_SENSORS: (undefined|!settings.Route),
 *   SITE_SETTINGS_SOUND: (undefined|!settings.Route),
 *   SITE_SETTINGS_LOCATION: (undefined|!settings.Route),
 *   SITE_SETTINGS_MICROPHONE: (undefined|!settings.Route),
 *   SITE_SETTINGS_MIDI_DEVICES: (undefined|!settings.Route),
 *   SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE: (undefined|!settings.Route),
 *   SITE_SETTINGS_NOTIFICATIONS: (undefined|!settings.Route),
 *   SITE_SETTINGS_PAYMENT_HANDLER: (undefined|!settings.Route),
 *   SITE_SETTINGS_PDF_DOCUMENTS: (undefined|!settings.Route),
 *   SITE_SETTINGS_POPUPS: (undefined|!settings.Route),
 *   SITE_SETTINGS_PROTECTED_CONTENT: (undefined|!settings.Route),
 *   SITE_SETTINGS_SITE_DATA: (undefined|!settings.Route),
 *   SITE_SETTINGS_SITE_DETAILS: (undefined|!settings.Route),
 *   SITE_SETTINGS_UNSANDBOXED_PLUGINS: (undefined|!settings.Route),
 *   SITE_SETTINGS_USB_DEVICES: (undefined|!settings.Route),
 *   SITE_SETTINGS_SERIAL_PORTS: (undefined|!settings.Route),
 *   SITE_SETTINGS_ZOOM_LEVELS: (undefined|!settings.Route),
 *   SMART_LOCK: (undefined|!settings.Route),
 *   SMB_SHARES: (undefined|!settings.Route),
 *   STORAGE: (undefined|!settings.Route),
 *   STYLUS: (undefined|!settings.Route),
 *   SYNC: (undefined|!settings.Route),
 *   SYNC_ADVANCED: (undefined|!settings.Route),
 *   SYSTEM: (undefined|!settings.Route),
 *   TRIGGERED_RESET_DIALOG: (undefined|!settings.Route),
 * }}
 */
let SettingsRoutes;

cr.define('settings', function() {

  /**
   * Class for navigable routes. May only be instantiated within this file.
   */
  class Route {
    /** @param {string} path */
    constructor(path) {
      /** @type {string} */
      this.path = path;

      /** @type {?settings.Route} */
      this.parent = null;

      /** @type {number} */
      this.depth = 0;

      /**
       * @type {boolean} Whether this route corresponds to a navigable
       *     dialog. Those routes don't belong to a "section".
       */
      this.isNavigableDialog = false;

      // Below are all legacy properties to provide compatibility with the old
      // routing system.

      /** @type {string} */
      this.section = '';
    }

    /**
     * Returns a new Route instance that's a child of this route.
     * @param {string} path Extends this route's path if it doesn't contain a
     *     leading slash.
     * @return {!settings.Route}
     * @private
     */
    createChild(path) {
      assert(path);

      // |path| extends this route's path if it doesn't have a leading slash.
      // If it does have a leading slash, it's just set as the new route's URL.
      const newUrl = path[0] == '/' ? path : `${this.path}/${path}`;

      const route = new Route(newUrl);
      route.parent = this;
      route.section = this.section;
      route.depth = this.depth + 1;

      return route;
    }

    /**
     * Returns a new Route instance that's a child section of this route.
     * TODO(tommycli): Remove once we've obsoleted the concept of sections.
     * @param {string} path
     * @param {string} section
     * @return {!settings.Route}
     * @private
     */
    createSection(path, section) {
      const route = this.createChild(path);
      route.section = section;
      return route;
    }

    /**
     * Returns the absolute path string for this Route, assuming this function
     * has been called from within chrome://settings.
     * @return {string}
     */
    getAbsolutePath() {
      return window.location.origin + this.path;
    }

    /**
     * Returns true if this route matches or is an ancestor of the parameter.
     * @param {!settings.Route} route
     * @return {boolean}
     */
    contains(route) {
      for (let r = route; r != null; r = r.parent) {
        if (this == r) {
          return true;
        }
      }
      return false;
    }

    /**
     * Returns true if this route is a subpage of a section.
     * @return {boolean}
     */
    isSubpage() {
      return !!this.parent && !!this.section &&
          this.parent.section == this.section;
    }
  }

  /**
   * @return {!SettingsRoutes} Routes that are shared between browser and OS
   *     settings under the same conditions (e.g. in guest mode).
   */
  function computeCommonRoutes() {
    /** @type {!SettingsRoutes} */
    const r = {};

    // Root pages.
    r.BASIC = new Route('/');
    r.ABOUT = new Route('/help');

    r.SIGN_OUT = r.BASIC.createChild('/signOut');
    r.SIGN_OUT.isNavigableDialog = true;

    r.SEARCH = r.BASIC.createSection('/search', 'search');
    if (!loadTimeData.getBoolean('isGuest')) {
      r.PEOPLE = r.BASIC.createSection('/people', 'people');
      r.SYNC = r.PEOPLE.createChild('/syncSetup');
      r.SYNC_ADVANCED = r.SYNC.createChild('/syncSetup/advanced');
    }

    return r;
  }

  /**
   * Adds Route objects for each path corresponding to browser-only content.
   * @param {!SettingsRoutes} r Routes to include browser-only content.
   */
  function addBrowserSettingsRoutes(r) {
    const pageVisibility = settings.pageVisibility || {};

    // <if expr="not chromeos">
    r.IMPORT_DATA = r.BASIC.createChild('/importData');
    r.IMPORT_DATA.isNavigableDialog = true;

    if (pageVisibility.people !== false) {
      r.MANAGE_PROFILE = r.PEOPLE.createChild('/manageProfile');
    }
    // </if>

    if (pageVisibility.appearance !== false) {
      r.APPEARANCE = r.BASIC.createSection('/appearance', 'appearance');
      r.FONTS = r.APPEARANCE.createChild('/fonts');
    }

    if (pageVisibility.autofill !== false) {
      r.AUTOFILL = r.BASIC.createSection('/autofill', 'autofill');
      r.PASSWORDS = r.AUTOFILL.createChild('/passwords');
      r.PAYMENTS = r.AUTOFILL.createChild('/payments');
      r.ADDRESSES = r.AUTOFILL.createChild('/addresses');
    }

    if (pageVisibility.defaultBrowser !== false) {
      r.DEFAULT_BROWSER =
          r.BASIC.createSection('/defaultBrowser', 'defaultBrowser');
    }

    r.SEARCH_ENGINES = r.SEARCH.createChild('/searchEngines');

    if (pageVisibility.onStartup !== false) {
      r.ON_STARTUP = r.BASIC.createSection('/onStartup', 'onStartup');
      r.STARTUP_PAGES = r.ON_STARTUP.createChild('/startupPages');
    }

    // Advanced Routes
    if (pageVisibility.advancedSettings !== false) {
      r.ADVANCED = new Route('/advanced');

      r.CLEAR_BROWSER_DATA = r.ADVANCED.createChild('/clearBrowserData');
      r.CLEAR_BROWSER_DATA.isNavigableDialog = true;

      if (pageVisibility.privacy !== false) {
        r.PRIVACY = r.ADVANCED.createSection('/privacy', 'privacy');
        r.CERTIFICATES = r.PRIVACY.createChild('/certificates');
        r.SITE_SETTINGS = r.PRIVACY.createChild('/content');
        if (loadTimeData.getBoolean('enableSecurityKeysSubpage')) {
          r.SECURITY_KEYS = r.PRIVACY.createChild('/securityKeys');
        }
      }

      r.SITE_SETTINGS_ALL = r.SITE_SETTINGS.createChild('all');
      r.SITE_SETTINGS_SITE_DETAILS =
          r.SITE_SETTINGS_ALL.createChild('/content/siteDetails');

      r.SITE_SETTINGS_HANDLERS = r.SITE_SETTINGS.createChild('/handlers');

      // TODO(tommycli): Find a way to refactor these repetitive category
      // routes.
      r.SITE_SETTINGS_ADS = r.SITE_SETTINGS.createChild('ads');
      r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS =
          r.SITE_SETTINGS.createChild('automaticDownloads');
      r.SITE_SETTINGS_BACKGROUND_SYNC =
          r.SITE_SETTINGS.createChild('backgroundSync');
      r.SITE_SETTINGS_CAMERA = r.SITE_SETTINGS.createChild('camera');
      r.SITE_SETTINGS_CLIPBOARD = r.SITE_SETTINGS.createChild('clipboard');
      r.SITE_SETTINGS_COOKIES = r.SITE_SETTINGS.createChild('cookies');
      r.SITE_SETTINGS_SITE_DATA =
          r.SITE_SETTINGS_COOKIES.createChild('/siteData');
      r.SITE_SETTINGS_DATA_DETAILS =
          r.SITE_SETTINGS_SITE_DATA.createChild('/cookies/detail');
      r.SITE_SETTINGS_IMAGES = r.SITE_SETTINGS.createChild('images');
      if (loadTimeData.getBoolean('enableInsecureContentContentSetting')) {
        r.SITE_SETTINGS_MIXEDSCRIPT =
            r.SITE_SETTINGS.createChild('insecureContent');
      }
      r.SITE_SETTINGS_JAVASCRIPT = r.SITE_SETTINGS.createChild('javascript');
      r.SITE_SETTINGS_SOUND = r.SITE_SETTINGS.createChild('sound');
      r.SITE_SETTINGS_SENSORS = r.SITE_SETTINGS.createChild('sensors');
      r.SITE_SETTINGS_LOCATION = r.SITE_SETTINGS.createChild('location');
      r.SITE_SETTINGS_MICROPHONE = r.SITE_SETTINGS.createChild('microphone');
      r.SITE_SETTINGS_NOTIFICATIONS =
          r.SITE_SETTINGS.createChild('notifications');
      r.SITE_SETTINGS_FLASH = r.SITE_SETTINGS.createChild('flash');
      r.SITE_SETTINGS_POPUPS = r.SITE_SETTINGS.createChild('popups');
      r.SITE_SETTINGS_UNSANDBOXED_PLUGINS =
          r.SITE_SETTINGS.createChild('unsandboxedPlugins');
      r.SITE_SETTINGS_MIDI_DEVICES = r.SITE_SETTINGS.createChild('midiDevices');
      r.SITE_SETTINGS_USB_DEVICES = r.SITE_SETTINGS.createChild('usbDevices');
      r.SITE_SETTINGS_SERIAL_PORTS = r.SITE_SETTINGS.createChild('serialPorts');
      r.SITE_SETTINGS_ZOOM_LEVELS = r.SITE_SETTINGS.createChild('zoomLevels');
      r.SITE_SETTINGS_PDF_DOCUMENTS =
          r.SITE_SETTINGS.createChild('pdfDocuments');
      r.SITE_SETTINGS_PROTECTED_CONTENT =
          r.SITE_SETTINGS.createChild('protectedContent');
      if (loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
        r.SITE_SETTINGS_PAYMENT_HANDLER =
            r.SITE_SETTINGS.createChild('paymentHandler');
      }
      if (loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures')) {
        r.SITE_SETTINGS_BLUETOOTH_SCANNING =
            r.SITE_SETTINGS.createChild('bluetoothScanning');
      }
      if (loadTimeData.getBoolean(
              'enableNativeFileSystemWriteContentSetting')) {
        r.SITE_SETTINGS_NATIVE_FILE_SYSTEM_WRITE =
            r.SITE_SETTINGS.createChild('filesystem');
      }

      r.LANGUAGES = r.ADVANCED.createSection('/languages', 'languages');
      // <if expr="not is_macosx">
      r.EDIT_DICTIONARY = r.LANGUAGES.createChild('/editDictionary');
      // </if>

      if (pageVisibility.downloads !== false) {
        r.DOWNLOADS = r.ADVANCED.createSection('/downloads', 'downloads');
      }

      r.PRINTING = r.ADVANCED.createSection('/printing', 'printing');
      r.CLOUD_PRINTERS = r.PRINTING.createChild('/cloudPrinters');

      r.ACCESSIBILITY = r.ADVANCED.createSection('/accessibility', 'a11y');

      // <if expr="chromeos or is_linux">
      if (loadTimeData.getBoolean('enableCaptionSettings')) {
        r.CAPTIONS = r.ACCESSIBILITY.createChild('/captions');
      }
      // </if>

      // <if expr="is_win">
      if (loadTimeData.getBoolean('enableCaptionSettings') &&
          !loadTimeData.getBoolean('isWindows10OrNewer')) {
        r.CAPTIONS = r.ACCESSIBILITY.createChild('/captions');
      }
      // </if>

      // <if expr="not chromeos">
      r.SYSTEM = r.ADVANCED.createSection('/system', 'system');
      // </if>

      if (pageVisibility.reset !== false) {
        r.RESET = r.ADVANCED.createSection('/reset', 'reset');
        r.RESET_DIALOG = r.ADVANCED.createChild('/resetProfileSettings');
        r.RESET_DIALOG.isNavigableDialog = true;
        r.TRIGGERED_RESET_DIALOG =
            r.ADVANCED.createChild('/triggeredResetProfileSettings');
        r.TRIGGERED_RESET_DIALOG.isNavigableDialog = true;
        // <if expr="_google_chrome and is_win">
        r.CHROME_CLEANUP = r.RESET.createChild('/cleanup');
        if (loadTimeData.getBoolean('showIncompatibleApplications')) {
          r.INCOMPATIBLE_APPLICATIONS =
              r.RESET.createChild('/incompatibleApplications');
        }
        // </if>
      }
    }
  }

  // <if expr="chromeos">
  /**
   * Adds Route objects for each path corresponding to CrOS-only content.
   * @param {!SettingsRoutes} r Routes to include CrOS-only content.
   */
  function addOSSettingsRoutes(r) {
    r.INTERNET = r.BASIC.createSection('/internet', 'internet');
    r.INTERNET_NETWORKS = r.INTERNET.createChild('/networks');
    r.NETWORK_DETAIL = r.INTERNET.createChild('/networkDetail');
    r.KNOWN_NETWORKS = r.INTERNET.createChild('/knownNetworks');
    r.BLUETOOTH = r.BASIC.createSection('/bluetooth', 'bluetooth');
    r.BLUETOOTH_DEVICES = r.BLUETOOTH.createChild('/bluetoothDevices');

    r.DEVICE = r.BASIC.createSection('/device', 'device');
    r.POINTERS = r.DEVICE.createChild('/pointer-overlay');
    r.KEYBOARD = r.DEVICE.createChild('/keyboard-overlay');
    r.STYLUS = r.DEVICE.createChild('/stylus');
    r.DISPLAY = r.DEVICE.createChild('/display');
    r.STORAGE = r.DEVICE.createChild('/storage');
    r.EXTERNAL_STORAGE_PREFERENCES =
        r.DEVICE.createChild('/storage/externalStoragePreferences');
    r.POWER = r.DEVICE.createChild('/power');

    // "About" is the only section in About, but we still need to create the
    // route in order to show the subpage on Chrome OS.
    r.ABOUT_ABOUT = r.ABOUT.createSection('/help/about', 'about');
    r.DETAILED_BUILD_INFO = r.ABOUT_ABOUT.createChild('/help/details');

    if (!loadTimeData.getBoolean('isGuest')) {
      r.MULTIDEVICE = r.BASIC.createSection('/multidevice', 'multidevice');
      r.MULTIDEVICE_FEATURES =
          r.MULTIDEVICE.createChild('/multidevice/features');
      r.SMART_LOCK =
          r.MULTIDEVICE_FEATURES.createChild('/multidevice/features/smartLock');

      r.ACCOUNTS = r.PEOPLE.createChild('/accounts');
      r.ACCOUNT_MANAGER = r.PEOPLE.createChild('/accountManager');
      r.KERBEROS_ACCOUNTS = r.PEOPLE.createChild('/kerberosAccounts');
      r.LOCK_SCREEN = r.PEOPLE.createChild('/lockScreen');
      r.FINGERPRINT = r.LOCK_SCREEN.createChild('/lockScreen/fingerprint');
    }

    // Show Android Apps page in the browser if split settings is turned off.
    if (!loadTimeData.getBoolean('isOSSettings') &&
        loadTimeData.getBoolean('showOSSettings') &&
        loadTimeData.valueExists('androidAppsVisible') &&
        loadTimeData.getBoolean('androidAppsVisible')) {
      r.ANDROID_APPS = r.BASIC.createSection('/androidApps', 'androidApps');
      r.ANDROID_APPS_DETAILS =
          r.ANDROID_APPS.createChild('/androidApps/details');
    }

    if (loadTimeData.valueExists('showCrostini') &&
        loadTimeData.getBoolean('showCrostini')) {
      r.CROSTINI = r.BASIC.createSection('/crostini', 'crostini');
      r.CROSTINI_ANDROID_ADB = r.CROSTINI.createChild('/crostini/androidAdb');
      r.CROSTINI_DETAILS = r.CROSTINI.createChild('/crostini/details');
      if (loadTimeData.valueExists('showCrostiniExportImport') &&
          loadTimeData.getBoolean('showCrostiniExportImport')) {
        r.CROSTINI_EXPORT_IMPORT =
            r.CROSTINI_DETAILS.createChild('/crostini/exportImport');
      }
      r.CROSTINI_SHARED_PATHS =
          r.CROSTINI_DETAILS.createChild('/crostini/sharedPaths');
      r.CROSTINI_SHARED_USB_DEVICES =
          r.CROSTINI_DETAILS.createChild('/crostini/sharedUsbDevices');
    }

    if (loadTimeData.valueExists('showPluginVm') &&
        loadTimeData.getBoolean('showPluginVm')) {
      r.PLUGIN_VM = r.BASIC.createSection('/pluginVm', 'pluginVm');
      r.PLUGIN_VM_DETAILS = r.PLUGIN_VM.createChild('/pluginVm/details');
      r.PLUGIN_VM_SHARED_PATHS =
          r.PLUGIN_VM.createChild('/pluginVm/sharedPaths');
    }

    r.GOOGLE_ASSISTANT = r.SEARCH.createChild('/googleAssistant');

    // This if/else accounts for sections that were added or refactored in
    // the settings split (crbug.com/950007) and some routes that were created
    // in browser settings conditioned on the pageVisibility constant, which is
    // being decoupled from OS Settings in the split. The 'else' block provides
    // a section-by-section comparison.
    // TODO (crbug.com/967861): Make 'if' block unconditional. Remove 'else'
    // block.
    if (loadTimeData.getBoolean('isOSSettings')) {
      r.ADVANCED = new Route('/advanced');

      r.PRIVACY = r.ADVANCED.createSection('/privacy', 'privacy');

      // Languages and input
      r.LANGUAGES = r.ADVANCED.createSection('/languages', 'languages');
      r.LANGUAGES_DETAILS = r.LANGUAGES.createChild('/languages/details');
      r.INPUT_METHODS =
          r.LANGUAGES_DETAILS.createChild('/languages/inputMethods');

      r.PRINTING = r.ADVANCED.createSection('/printing', 'printing');

      r.ACCESSIBILITY = r.ADVANCED.createSection('/accessibility', 'a11y');

      if (!loadTimeData.getBoolean('isGuest')) {
        if (loadTimeData.valueExists('splitSettingsSyncEnabled') &&
            loadTimeData.getBoolean('splitSettingsSyncEnabled')) {
          r.OS_SYNC = r.PEOPLE.createChild('/osSync');
        }
        // Personalization
        r.PERSONALIZATION =
            r.BASIC.createSection('/personalization', 'personalization');
        r.CHANGE_PICTURE = r.PERSONALIZATION.createChild('/changePicture');

        // Files (analogous to Downloads)
        r.FILES = r.ADVANCED.createSection('/files', 'files');
        r.SMB_SHARES = r.FILES.createChild('/smbShares');
      }

      // Reset
      if (loadTimeData.valueExists('allowPowerwash') &&
          loadTimeData.getBoolean('allowPowerwash')) {
        r.RESET = r.ADVANCED.createSection('/reset', 'reset');
      }

      const showAppManagement = loadTimeData.valueExists('showAppManagement') &&
          loadTimeData.getBoolean('showAppManagement');
      const showAndroidApps = loadTimeData.valueExists('androidAppsVisible') &&
          loadTimeData.getBoolean('androidAppsVisible');
      // Apps
      if (showAppManagement || showAndroidApps) {
        r.APPS = r.BASIC.createSection('/apps', 'apps');
        if (showAppManagement) {
          r.APP_MANAGEMENT = r.APPS.createChild('/app-management');
          r.APP_MANAGEMENT_DETAIL =
              r.APP_MANAGEMENT.createChild('/app-management/detail');
        }
        if (showAndroidApps) {
          r.ANDROID_APPS_DETAILS = r.APPS.createChild('/androidAppsDetails');
        }
      }
    } else {
      assert(r.ADVANCED, 'ADVANCED route should exist');

      assert(r.PRIVACY, 'PRIVACY route should exist');

      // Languages and input
      assert(r.LANGUAGES, 'LANGUAGES route should exist');
      r.INPUT_METHODS = r.LANGUAGES.createChild('/inputMethods');

      assert(r.PRINTING, 'PRINTING route should exist');

      assert(r.ACCESSIBILITY, 'ACCESSIBILITY route should exist');

      if (!loadTimeData.getBoolean('isGuest')) {
        // People
        r.CHANGE_PICTURE = r.PEOPLE.createChild('/changePicture');

        // Downloads (analogous to Files)
        assert(r.DOWNLOADS, 'DOWNLOADS route should exist');
        r.SMB_SHARES = r.DOWNLOADS.createChild('/smbShares');

        // Reset
        assert(r.RESET, 'RESET route should exist');
      }

      assert(!r.APPS, 'APPS route should not exist');
    }

    r.DATETIME = r.ADVANCED.createSection('/dateTime', 'dateTime');
    r.DATETIME_TIMEZONE_SUBPAGE = r.DATETIME.createChild('/dateTime/timeZone');

    r.CUPS_PRINTERS = r.PRINTING.createChild('/cupsPrinters');

    r.MANAGE_ACCESSIBILITY =
        r.ACCESSIBILITY.createChild('/manageAccessibility');
    if (loadTimeData.getBoolean('showExperimentalAccessibilitySwitchAccess')) {
      r.MANAGE_SWITCH_ACCESS_SETTINGS = r.MANAGE_ACCESSIBILITY.createChild(
          '/manageAccessibility/switchAccess');
    }
    r.MANAGE_TTS_SETTINGS =
        r.MANAGE_ACCESSIBILITY.createChild('/manageAccessibility/tts');

    r.MANAGE_CAPTION_SETTINGS =
        r.MANAGE_ACCESSIBILITY.createChild('/manageAccessibility/captions');
  }
  // </if>

  class Router {
    /** @param {!SettingsRoutes} availableRoutes */
    constructor(availableRoutes) {
      /**
       * List of available routes. This is populated taking into account current
       * state (like guest mode).
       * @private {!SettingsRoutes}
       */
      this.routes_ = availableRoutes;

      /**
       * The current active route. This updated is only by settings.navigateTo
       * or settings.initializeRouteFromUrl.
       * @type {!settings.Route}
       */
      this.currentRoute = /** @type {!settings.Route} */ (this.routes_.BASIC);

      /**
       * The current query parameters. This is updated only by
       * settings.navigateTo or settings.initializeRouteFromUrl.
       * @private {!URLSearchParams}
       */
      this.currentQueryParameters_ = new URLSearchParams();

      /** @private {boolean} */
      this.wasLastRouteChangePopstate_ = false;

      /** @private {boolean}*/
      this.initializeRouteFromUrlCalled_ = false;
    }

    /** @return {settings.Route} */
    getRoute(routeName) {
      return this.routes_[routeName];
    }

    /** @return {!SettingsRoutes} */
    getRoutes() {
      return this.routes_;
    }

    /**
     * Helper function to set the current route and notify all observers.
     * @param {!settings.Route} route
     * @param {!URLSearchParams} queryParameters
     * @param {boolean} isPopstate
     */
    setCurrentRoute(route, queryParameters, isPopstate) {
      this.recordMetrics(route.path);

      const oldRoute = this.currentRoute;
      this.currentRoute = route;
      this.currentQueryParameters_ = queryParameters;
      this.wasLastRouteChangePopstate_ = isPopstate;
      new Set(routeObservers).forEach((observer) => {
        observer.currentRouteChanged(this.currentRoute, oldRoute);
      });
    }

    /** @return {!settings.Route} */
    getCurrentRoute() {
      return this.currentRoute;
    }

    /** @return {!URLSearchParams} */
    getQueryParameters() {
      return new URLSearchParams(
          this.currentQueryParameters_);  // Defensive copy.
    }

    /** @return {boolean} */
    lastRouteChangeWasPopstate() {
      return this.wasLastRouteChangePopstate_;
    }

    /**
     * @param {string} path
     * @return {?settings.Route} The matching canonical route, or null if none
     *     matches.
     */
    getRouteForPath(path) {
      // Allow trailing slash in paths.
      const canonicalPath = path.replace(CANONICAL_PATH_REGEX, '$1$2');

      // TODO(tommycli): Use Object.values once Closure compilation supports it.
      const matchingKey =
          Object.keys(this.routes_)
              .find((key) => this.routes_[key].path == canonicalPath);

      return matchingKey ? this.routes_[matchingKey] : null;
    }

    /**
     * Navigates to a canonical route and pushes a new history entry.
     * @param {!settings.Route} route
     * @param {URLSearchParams=} opt_dynamicParameters Navigations to the same
     *     URL parameters in a different order will still push to history.
     * @param {boolean=} opt_removeSearch Whether to strip the 'search' URL
     *     parameter during navigation. Defaults to false.
     */
    navigateTo(route, opt_dynamicParameters, opt_removeSearch) {
      // The ADVANCED route only serves as a parent of subpages, and should not
      // be possible to navigate to it directly.
      if (route == this.routes_.ADVANCED) {
        route = /** @type {!settings.Route} */ (this.routes_.BASIC);
      }

      const params = opt_dynamicParameters || new URLSearchParams();
      const removeSearch = !!opt_removeSearch;

      const oldSearchParam = this.getQueryParameters().get('search') || '';
      const newSearchParam = params.get('search') || '';

      if (!removeSearch && oldSearchParam && !newSearchParam) {
        params.append('search', oldSearchParam);
      }

      let url = route.path;
      const queryString = params.toString();
      if (queryString) {
        url += '?' + queryString;
      }

      // History serializes the state, so we don't push the actual route object.
      window.history.pushState(this.currentRoute.path, '', url);
      this.setCurrentRoute(route, params, false);
    }

    /**
     * Navigates to the previous route if it has an equal or lesser depth.
     * If there is no previous route in history meeting those requirements,
     * this navigates to the immediate parent. This will never exit Settings.
     */
    navigateToPreviousRoute() {
      const previousRoute = window.history.state &&
          assert(this.getRouteForPath(
              /** @type {string} */ (window.history.state)));

      if (previousRoute && previousRoute.depth <= this.currentRoute.depth) {
        window.history.back();
      } else {
        this.navigateTo(
            this.currentRoute.parent ||
            /** @type {!settings.Route} */ (this.routes_.BASIC));
      }
    }

    /**
     * Initialize the route and query params from the URL.
     */
    initializeRouteFromUrl() {
      assert(!this.initializeRouteFromUrlCalled_);
      this.initializeRouteFromUrlCalled_ = true;

      const route = this.getRouteForPath(window.location.pathname);

      // Record all correct paths entered on the settings page, and
      // as all incorrect paths are routed to the main settings page,
      // record all incorrect paths as hitting the main settings page.
      this.recordMetrics(route ? route.path : this.routes_.BASIC.path);

      // Never allow direct navigation to ADVANCED.
      if (route && route != this.routes_.ADVANCED) {
        this.currentRoute = route;
        this.currentQueryParameters_ =
            new URLSearchParams(window.location.search);
      } else {
        window.history.replaceState(undefined, '', this.routes_.BASIC.path);
      }
    }

    /**
     * Make a UMA note about visiting this URL path.
     * @param {string} urlPath The url path (only).
     */
    recordMetrics(urlPath) {
      assert(!urlPath.startsWith('chrome://'));
      assert(!urlPath.startsWith('settings'));
      assert(urlPath.startsWith('/'));
      assert(!urlPath.match(/\?/g));
      chrome.metricsPrivate.recordSparseHashable(
          'WebUI.Settings.PathVisited', urlPath);
    }

    resetRouteForTesting() {
      this.initializeRouteFromUrlCalled_ = false;
      this.wasLastRouteChangePopstate_ = false;
      this.currentRoute = /** @type {!settings.Route} */ (this.routes_.BASIC);
      this.currentQueryParameters_ = new URLSearchParams();
    }
  }

  /**
   * @return {!settings.Router} A router with at least those routes common to OS
   *     and browser settings. If the window is not in OS settings (based on
   *     loadTimeData) then browser specific routes are added. If the window is
   *     OS settings or if Chrome OS is using a consolidated settings page for
   *     OS and browser settings then OS specific routes are added.
   */
  function buildRouter() {
    const availableRoutes = computeCommonRoutes();
    const isOSSettings = loadTimeData.valueExists('isOSSettings') &&
        loadTimeData.getBoolean('isOSSettings');
    if (!isOSSettings) {
      addBrowserSettingsRoutes(availableRoutes);
    }

    // <if expr="chromeos">
    const showOSSettings = loadTimeData.valueExists('showOSSettings') &&
        loadTimeData.getBoolean('showOSSettings');
    if (isOSSettings || showOSSettings) {
      addOSSettingsRoutes(availableRoutes);
    }
    // </if>
    return new Router(availableRoutes);
  }
  const routerInstance = buildRouter();

  const routeObservers = new Set();

  /** @polymerBehavior */
  const RouteObserverBehavior = {
    /** @override */
    attached: function() {
      assert(!routeObservers.has(this));
      routeObservers.add(this);

      // Emulating Polymer data bindings, the observer is called when the
      // element starts observing the route.
      this.currentRouteChanged(routerInstance.currentRoute, undefined);
    },

    /** @override */
    detached: function() {
      assert(routeObservers.delete(this));
    },

    /**
     * @param {!settings.Route|undefined} opt_newRoute
     * @param {!settings.Route|undefined} opt_oldRoute
     */
    currentRouteChanged: function(opt_newRoute, opt_oldRoute) {
      assertNotReached();
    },
  };

  /**
   * Regular expression that captures the leading slash, the content and the
   * trailing slash in three different groups.
   * @type {!RegExp}
   */
  const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;

  window.addEventListener('popstate', function(event) {
    // On pop state, do not push the state onto the window.history again.
    routerInstance.setCurrentRoute(
        /** @type {!settings.Route} */ (
            routerInstance.getRouteForPath(window.location.pathname) ||
            routerInstance.getRoutes().BASIC),
        new URLSearchParams(window.location.search), true);
  });

  // TODO(dpapad): Change to 'get routes() {}' in export when we fix a bug in
  // ChromePass that limits the syntax of what can be returned from cr.define().
  const routes = routerInstance.getRoutes();

  // TODO(dpapad): Stop exposing all those methods directly on settings.*,
  // and instead update all clients to use the singleton instance directly
  const getCurrentRoute = routerInstance.getCurrentRoute.bind(routerInstance);
  const getRouteForPath = routerInstance.getRouteForPath.bind(routerInstance);
  const initializeRouteFromUrl =
      routerInstance.initializeRouteFromUrl.bind(routerInstance);
  const resetRouteForTesting =
      routerInstance.resetRouteForTesting.bind(routerInstance);
  const getQueryParameters =
      routerInstance.getQueryParameters.bind(routerInstance);
  const lastRouteChangeWasPopstate =
      routerInstance.lastRouteChangeWasPopstate.bind(routerInstance);
  const navigateTo = routerInstance.navigateTo.bind(routerInstance);
  const navigateToPreviousRoute =
      routerInstance.navigateToPreviousRoute.bind(routerInstance);

  return {
    Route: Route,            // The Route class definition.
    Router: Router,          // The Router class definition.
    router: routerInstance,  // the singleton.
    buildRouterForTesting: buildRouter,
    routes: routes,
    RouteObserverBehavior: RouteObserverBehavior,
    getRouteForPath: getRouteForPath,
    initializeRouteFromUrl: initializeRouteFromUrl,
    resetRouteForTesting: resetRouteForTesting,
    getCurrentRoute: getCurrentRoute,
    getQueryParameters: getQueryParameters,
    lastRouteChangeWasPopstate: lastRouteChangeWasPopstate,
    navigateTo: navigateTo,
    navigateToPreviousRoute: navigateToPreviousRoute,
  };
});
