// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {pageVisibility} from './page_visibility.js';
import {Route, Router} from './router.js';
import {SettingsRoutes} from './settings_routes.js';

/**
 * Add all of the child routes that originate from the privacy route,
 * regardless of whether the privacy section under basic or advanced.
 * @param {!SettingsRoutes} r
 */
function addPrivacyChildRoutes(r) {
  r.SITE_SETTINGS = r.PRIVACY.createChild('/content');
  r.COOKIES = r.PRIVACY.createChild('/cookies');
  r.SECURITY = r.PRIVACY.createChild('/security');

  // TODO(crbug.com/1147032): The certificates settings page is temporarily
  // disabled for Lacros-Chrome until a better solution is found.
  // <if expr="use_nss_certs and not lacros">
  r.CERTIFICATES = r.SECURITY.createChild('/certificates');
  // </if>

  if (loadTimeData.getBoolean('enableSecurityKeysSubpage')) {
    r.SECURITY_KEYS = r.SECURITY.createChild('/securityKeys');
  }

  r.SITE_SETTINGS_ALL = r.SITE_SETTINGS.createChild('all');
  r.SITE_SETTINGS_SITE_DETAILS =
      r.SITE_SETTINGS_ALL.createChild('/content/siteDetails');

  r.SITE_SETTINGS_HANDLERS = r.SITE_SETTINGS.createChild('/handlers');

  // TODO(tommycli): Find a way to refactor these repetitive category
  // routes.
  r.SITE_SETTINGS_ADS = r.SITE_SETTINGS.createChild('ads');
  r.SITE_SETTINGS_AR = r.SITE_SETTINGS.createChild('ar');
  r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS =
      r.SITE_SETTINGS.createChild('automaticDownloads');
  r.SITE_SETTINGS_BACKGROUND_SYNC =
      r.SITE_SETTINGS.createChild('backgroundSync');
  r.SITE_SETTINGS_CAMERA = r.SITE_SETTINGS.createChild('camera');
  r.SITE_SETTINGS_CLIPBOARD = r.SITE_SETTINGS.createChild('clipboard');
  r.SITE_SETTINGS_SITE_DATA = r.COOKIES.createChild('/siteData');
  r.SITE_SETTINGS_DATA_DETAILS =
      r.SITE_SETTINGS_SITE_DATA.createChild('/cookies/detail');
  r.SITE_SETTINGS_IDLE_DETECTION = r.SITE_SETTINGS.createChild('idleDetection');
  r.SITE_SETTINGS_IMAGES = r.SITE_SETTINGS.createChild('images');
  r.SITE_SETTINGS_MIXEDSCRIPT = r.SITE_SETTINGS.createChild('insecureContent');
  r.SITE_SETTINGS_JAVASCRIPT = r.SITE_SETTINGS.createChild('javascript');
  r.SITE_SETTINGS_SOUND = r.SITE_SETTINGS.createChild('sound');
  r.SITE_SETTINGS_SENSORS = r.SITE_SETTINGS.createChild('sensors');
  r.SITE_SETTINGS_LOCATION = r.SITE_SETTINGS.createChild('location');
  r.SITE_SETTINGS_MICROPHONE = r.SITE_SETTINGS.createChild('microphone');
  r.SITE_SETTINGS_NOTIFICATIONS = r.SITE_SETTINGS.createChild('notifications');
  r.SITE_SETTINGS_POPUPS = r.SITE_SETTINGS.createChild('popups');
  r.SITE_SETTINGS_MIDI_DEVICES = r.SITE_SETTINGS.createChild('midiDevices');
  r.SITE_SETTINGS_USB_DEVICES = r.SITE_SETTINGS.createChild('usbDevices');
  r.SITE_SETTINGS_HID_DEVICES = r.SITE_SETTINGS.createChild('hidDevices');
  r.SITE_SETTINGS_SERIAL_PORTS = r.SITE_SETTINGS.createChild('serialPorts');
  if (loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend')) {
    r.SITE_SETTINGS_BLUETOOTH_DEVICES =
        r.SITE_SETTINGS.createChild('bluetoothDevices');
  }
  r.SITE_SETTINGS_ZOOM_LEVELS = r.SITE_SETTINGS.createChild('zoomLevels');
  r.SITE_SETTINGS_PDF_DOCUMENTS = r.SITE_SETTINGS.createChild('pdfDocuments');
  r.SITE_SETTINGS_PROTECTED_CONTENT =
      r.SITE_SETTINGS.createChild('protectedContent');
  if (loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
    r.SITE_SETTINGS_PAYMENT_HANDLER =
        r.SITE_SETTINGS.createChild('paymentHandler');
  }
  r.SITE_SETTINGS_VR = r.SITE_SETTINGS.createChild('vr');
  if (loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures')) {
    r.SITE_SETTINGS_BLUETOOTH_SCANNING =
        r.SITE_SETTINGS.createChild('bluetoothScanning');
    r.SITE_SETTINGS_WINDOW_PLACEMENT =
        r.SITE_SETTINGS.createChild('windowPlacement');
  }
  r.SITE_SETTINGS_FILE_SYSTEM_WRITE = r.SITE_SETTINGS.createChild('filesystem');
  if (loadTimeData.getBoolean('enableFontAccessContentSetting')) {
    r.SITE_SETTINGS_FONT_ACCESS = r.SITE_SETTINGS.createChild('fontAccess');
  }
  if (loadTimeData.getBoolean('enableFileHandlingContentSetting')) {
    r.SITE_SETTINGS_FILE_HANDLING = r.SITE_SETTINGS.createChild('fileHandlers');
  }
}

/**
 * Adds Route objects for each path.
 * @return {!SettingsRoutes}
 */
function createBrowserSettingsRoutes() {
  const r = /** @type {!SettingsRoutes} */ ({});

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

  const visibility = pageVisibility || {};

  // <if expr="not chromeos">
  r.IMPORT_DATA = r.BASIC.createChild('/importData');
  r.IMPORT_DATA.isNavigableDialog = true;

  if (visibility.people !== false) {
    r.MANAGE_PROFILE = r.PEOPLE.createChild('/manageProfile');
  }
  // </if>

  if (visibility.appearance !== false) {
    r.APPEARANCE = r.BASIC.createSection('/appearance', 'appearance');
    r.FONTS = r.APPEARANCE.createChild('/fonts');
  }

  if (visibility.autofill !== false) {
    r.AUTOFILL = r.BASIC.createSection('/autofill', 'autofill');
    r.PASSWORDS = r.AUTOFILL.createChild('/passwords');
    r.CHECK_PASSWORDS = r.PASSWORDS.createChild('check');

    if (loadTimeData.getBoolean('enableAccountStorage')) {
      r.DEVICE_PASSWORDS = r.PASSWORDS.createChild('device');
    }

    r.PAYMENTS = r.AUTOFILL.createChild('/payments');
    r.ADDRESSES = r.AUTOFILL.createChild('/addresses');
  }

  r.CLEAR_BROWSER_DATA = r.BASIC.createChild('/clearBrowserData');
  r.CLEAR_BROWSER_DATA.isNavigableDialog = true;

  if (visibility.privacy !== false) {
    r.PRIVACY = r.BASIC.createSection('/privacy', 'privacy');
    addPrivacyChildRoutes(r);

    r.SAFETY_CHECK = r.BASIC.createSection('/safetyCheck', 'safetyCheck');
  }

  // <if expr="not chromeos and not lacros">
  if (visibility.defaultBrowser !== false) {
    r.DEFAULT_BROWSER =
        r.BASIC.createSection('/defaultBrowser', 'defaultBrowser');
  }
  // </if>

  r.SEARCH_ENGINES = r.SEARCH.createChild('/searchEngines');

  if (visibility.onStartup !== false) {
    r.ON_STARTUP = r.BASIC.createSection('/onStartup', 'onStartup');
    r.STARTUP_PAGES = r.ON_STARTUP.createChild('/startupPages');
  }

  // Advanced Routes
  if (visibility.advancedSettings !== false) {
    r.ADVANCED = new Route('/advanced');

    r.LANGUAGES = r.ADVANCED.createSection('/languages', 'languages');
    // <if expr="not is_macosx">
    r.EDIT_DICTIONARY = r.LANGUAGES.createChild('/editDictionary');
    // </if>
    // <if expr="not chromeos and not lacros">
    if (loadTimeData.getBoolean('enableDesktopRestructuredLanguageSettings')) {
      r.LANGUAGE_SETTINGS = r.LANGUAGES.createChild('/languageSettings');
    }
    // </if>

    if (visibility.downloads !== false) {
      r.DOWNLOADS = r.ADVANCED.createSection('/downloads', 'downloads');
    }

    r.ACCESSIBILITY = r.ADVANCED.createSection('/accessibility', 'a11y');

    // <if expr="chromeos or is_linux">
    r.CAPTIONS = r.ACCESSIBILITY.createChild('/captions');
    // </if>

    // <if expr="is_win">
    if (!loadTimeData.getBoolean('isWindows10OrNewer')) {
      r.CAPTIONS = r.ACCESSIBILITY.createChild('/captions');
    }
    // </if>

    // <if expr="not chromeos and not lacros">
    r.SYSTEM = r.ADVANCED.createSection('/system', 'system');
    // </if>

    if (visibility.reset !== false) {
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
  return r;
}

/**
 * @return {!Router} A router with the browser settings routes.
 */
export function buildRouter() {
  return new Router(createBrowserSettingsRoutes());
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

export const routes =
    /** @type {!SettingsRoutes} */ (Router.getInstance().getRoutes());
