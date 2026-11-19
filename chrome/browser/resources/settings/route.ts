// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';

import {loadTimeData} from './i18n_setup.js';
import {pageVisibility} from './page_visibility.js';
import type {SettingsRoutes} from './router.js';
import {Route, Router} from './router.js';

// LINT.IfChange(AiSettingsSection)
export enum AiSettingsSection {
  NONE = '',
  GEMINI = 'gemini',
  GEMINI_ENTERPRISE = 'geminiEnterprise',
}
// LINT.ThenChange(//chrome/browser/glic/public/glic_provider_spec.h:AiSettingsSection)

// <if expr="_google_chrome">
/** Returns the icon that the Gemini rows show in Google Chrome. */
function getSparkIcon(): string {
  return loadTimeData.getBoolean('glicAssetsV2Enabled') ?
      'settings-internal:sparkv2' :
      'settings-internal:spark';
}
// </if>

/**
 * Add all of the child routes that originate from the privacy route,
 * regardless of whether the privacy section under basic or advanced.
 */
function addPrivacyChildRoutes(r: Partial<SettingsRoutes>) {
  assert(r.PRIVACY);
  r.CLEAR_BROWSER_DATA =
      r.PRIVACY.createChild('/clearBrowserData', 'cr:delete');
  r.CLEAR_BROWSER_DATA.isNavigableDialog = true;

  const visibility = pageVisibility || {};

  if (visibility.safetyHub !== false) {
    r.SAFETY_HUB = r.PRIVACY.createChild('/safetyCheck');
  }

  if (loadTimeData.getBoolean('showPrivacyGuide')) {
    r.PRIVACY_GUIDE = r.PRIVACY.createChild('guide', 'privacy20:signpost');
  }

  r.SITE_SETTINGS = r.PRIVACY.createChild('/content', 'privacy:page-info');

  r.SECURITY = r.PRIVACY.createChild('/security', 'privacy:lock');

  r.COOKIES = r.PRIVACY.createChild('/cookies', 'privacy:cookie');

  if (loadTimeData.getBoolean('enableSecurityKeysSubpage')) {
    r.SECURITY_KEYS = r.SECURITY.createChild('/securityKeys');
  }

  r.SITE_SETTINGS_ALL = r.SITE_SETTINGS.createChild('all');
  r.SITE_SETTINGS_SITE_DETAILS =
      r.SITE_SETTINGS_ALL.createChild('/content/siteDetails');

  r.SITE_SETTINGS_HANDLERS =
      r.SITE_SETTINGS.createChild('/handlers', 'privacy:protocol-handler');

  // TODO(tommycli): Find a way to refactor these repetitive category
  // routes.
  r.SITE_SETTINGS_ADS = r.SITE_SETTINGS.createChild('ads', 'privacy:web-asset');
  r.SITE_SETTINGS_AR = r.SITE_SETTINGS.createChild('ar', 'privacy:cardboard');
  r.SITE_SETTINGS_AUTOMATIC_DOWNLOADS =
      r.SITE_SETTINGS.createChild('automaticDownloads', 'cr:download');
  r.SITE_SETTINGS_AUTO_PICTURE_IN_PICTURE = r.SITE_SETTINGS.createChild(
      'autoPictureInPicture', 'settings:picture-in-picture');
  if (loadTimeData.getBoolean('enableCapturedSurfaceControl')) {
    r.SITE_SETTINGS_CAPTURED_SURFACE_CONTROL = r.SITE_SETTINGS.createChild(
        'capturedSurfaceControl', 'settings:touchpad-mouse');
  }
  // <if expr="enable_smart_card">
  if (loadTimeData.getBoolean('enableSmartCardReadersContentSetting')) {
    r.SITE_SETTINGS_SMART_CARD_READERS = r.SITE_SETTINGS.createChild(
        'smartCardReaders', 'privacy:smart-card-reader');
  }
  // </if>
  // <if expr="is_chromeos">
  if (loadTimeData.getBoolean('enableWebPrintingContentSetting')) {
    r.SITE_SETTINGS_WEB_PRINTING =
        r.SITE_SETTINGS.createChild('webPrinting', 'settings:print');
  }
  // </if>
  r.SITE_SETTINGS_AUTO_VERIFY =
      r.SITE_SETTINGS.createChild('autoVerify', 'privacy20:person-check');
  r.SITE_SETTINGS_BACKGROUND_SYNC =
      r.SITE_SETTINGS.createChild('backgroundSync', 'cr:sync');
  r.SITE_SETTINGS_CAMERA = r.SITE_SETTINGS.createChild('camera', 'cr:videocam');
  r.SITE_SETTINGS_CLIPBOARD =
      r.SITE_SETTINGS.createChild('clipboard', 'privacy:content-paste');
  if (loadTimeData.getBoolean('enableHandTrackingContentSetting')) {
    r.SITE_SETTINGS_HAND_TRACKING =
        r.SITE_SETTINGS.createChild('handTracking', 'privacy:hand-gesture');
  }
  r.SITE_SETTINGS_IDLE_DETECTION =
      r.SITE_SETTINGS.createChild('idleDetection', 'settings:devices');
  r.SITE_SETTINGS_IMAGES =
      r.SITE_SETTINGS.createChild('images', 'privacy:imagesmode');
  r.SITE_SETTINGS_MIXEDSCRIPT =
      r.SITE_SETTINGS.createChild('insecureContent', 'privacy:warning');
  r.SITE_SETTINGS_JAVASCRIPT =
      r.SITE_SETTINGS.createChild('javascript', 'privacy:code');
  r.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER =
      r.SITE_SETTINGS.createChild('v8', 'privacy:v8');
  if (loadTimeData.getBoolean('enableKeyboardLockPrompt')) {
    r.SITE_SETTINGS_KEYBOARD_LOCK =
        r.SITE_SETTINGS.createChild('keyboardLock', 'settings20:keyboard-lock');
  }
  r.SITE_SETTINGS_SOUND =
      r.SITE_SETTINGS.createChild('sound', 'privacy:volume-up');
  r.SITE_SETTINGS_SENSORS =
      r.SITE_SETTINGS.createChild('sensors', 'privacy:sensors');
  r.SITE_SETTINGS_LOCATION =
      r.SITE_SETTINGS.createChild('location', 'settings:location-on');
  r.SITE_SETTINGS_MICROPHONE =
      r.SITE_SETTINGS.createChild('microphone', 'privacy:mic');
  r.SITE_SETTINGS_NOTIFICATIONS =
      r.SITE_SETTINGS.createChild('notifications', 'privacy:notifications');
  r.SITE_SETTINGS_POPUPS =
      r.SITE_SETTINGS.createChild('popups', 'cr:open-in-new');
  r.SITE_SETTINGS_MIDI_DEVICES =
      r.SITE_SETTINGS.createChild('midiDevices', 'privacy:piano');
  r.SITE_SETTINGS_USB_DEVICES =
      r.SITE_SETTINGS.createChild('usbDevices', 'privacy:usb');
  r.SITE_SETTINGS_HID_DEVICES =
      r.SITE_SETTINGS.createChild('hidDevices', 'privacy:videogame-asset');
  r.SITE_SETTINGS_SERIAL_PORTS =
      r.SITE_SETTINGS.createChild('serialPorts', 'privacy:developer-board');
  if (loadTimeData.getBoolean('enableWebBluetoothNewPermissionsBackend')) {
    r.SITE_SETTINGS_BLUETOOTH_DEVICES =
        r.SITE_SETTINGS.createChild('bluetoothDevices', 'settings:bluetooth');
  }
  r.SITE_SETTINGS_ZOOM_LEVELS =
      r.SITE_SETTINGS.createChild('zoomLevels', 'privacy:zoom-in');
  r.SITE_SETTINGS_PDF_DOCUMENTS =
      r.SITE_SETTINGS.createChild('pdfDocuments', 'privacy:drive-pdf');
  r.SITE_SETTINGS_PROTECTED_CONTENT = r.SITE_SETTINGS.createChild(
      'protectedContent', 'privacy:sync-saved-locally');
  if (loadTimeData.getBoolean('enablePaymentHandlerContentSetting')) {
    r.SITE_SETTINGS_PAYMENT_HANDLER =
        r.SITE_SETTINGS.createChild('paymentHandler', 'privacy:credit-card');
  }
  if (loadTimeData.getBoolean('enableFederatedIdentityApiContentSetting')) {
    r.SITE_SETTINGS_FEDERATED_IDENTITY_API = r.SITE_SETTINGS.createChild(
        'federatedIdentityApi', 'privacy:account-circle');
  }
  r.SITE_SETTINGS_SITE_DATA =
      r.SITE_SETTINGS.createChild('siteData', 'privacy:database');
  r.SITE_SETTINGS_VR = r.SITE_SETTINGS.createChild('vr', 'privacy:cardboard');
  if (loadTimeData.getBoolean('enableExperimentalWebPlatformFeatures')) {
    r.SITE_SETTINGS_BLUETOOTH_SCANNING = r.SITE_SETTINGS.createChild(
        'bluetoothScanning', 'settings:bluetooth-searching');
  }

  r.SITE_SETTINGS_WINDOW_MANAGEMENT =
      r.SITE_SETTINGS.createChild('windowManagement', 'privacy:select-window');
  r.SITE_SETTINGS_FILE_SYSTEM_WRITE =
      r.SITE_SETTINGS.createChild('filesystem', 'privacy:file-save');
  if (loadTimeData.getBoolean('enablePersistentPermissions')) {
    r.SITE_SETTINGS_FILE_SYSTEM_WRITE_DETAILS =
        r.SITE_SETTINGS_FILE_SYSTEM_WRITE.createChild('siteDetails');
  }
  r.SITE_SETTINGS_LOCAL_FONTS =
      r.SITE_SETTINGS.createChild('localFonts', 'privacy:font-download');
  r.SITE_SETTINGS_STORAGE_ACCESS =
      r.SITE_SETTINGS.createChild('storageAccess', 'privacy:vr180-create2d');
  r.SITE_SETTINGS_AUTOMATIC_FULLSCREEN =
      r.SITE_SETTINGS.createChild('automaticFullScreen', 'cr:fullscreen');
  if (loadTimeData.getBoolean('enableWebAppInstallation')) {
    r.SITE_SETTINGS_WEB_APP_INSTALLATION = r.SITE_SETTINGS.createChild(
        'webAppInstallations', 'settings:install-desktop');
  }
  if (loadTimeData.getBoolean('enableLocalNetworkAccessSetting')) {
    r.SITE_SETTINGS_LOCAL_NETWORK =
        r.SITE_SETTINGS.createChild('localNetwork', 'settings20:router');
    r.SITE_SETTINGS_LOOPBACK_NETWORK = r.SITE_SETTINGS.createChild(
        'loopbackNetwork', 'settings20:desktop-windows');
  }
}

/**
 * Add all of the child routes that originate from the people/home route.
 */
function addSharedPeopleRoutes(r: Partial<SettingsRoutes>, parent: Route) {
  r.ACCOUNT = parent.createChild('/account');
  r.GOOGLE_SERVICES = parent.createChild('/googleServices');
  if (loadTimeData.getBoolean('enableDeviceTabVisibilitySettings')) {
    // TODO(crbug.com/570417844): Finalize the settings URL path with UX/PM
    // before launch.
    r.DEVICE_TAB_VISIBILITY = r.ACCOUNT.createChild('/deviceTabVisibility');
  }
  // <if expr="not is_chromeos">
  r.SIGN_OUT = parent.createChild('/signOut');
  r.SIGN_OUT.isNavigableDialog = true;
  r.IMPORT_DATA = parent.createChild('/importData');
  r.IMPORT_DATA.isNavigableDialog = true;
  r.MANAGE_PROFILE = parent.createChild('/manageProfile');
  // </if>

  r.SYNC = parent.createChild('/syncSetup');
  r.SYNC_ADVANCED = r.SYNC.createChild('/syncSetup/advanced');
}

/**
 * Adds Route objects for each path.
 */
function createRoutes(): SettingsRoutes {
  const r: Partial<SettingsRoutes> = {};
  const isSettingsRefresh2026Enabled =
      loadTimeData.getString('settingsRefresh2026') !== '';

  // Root page.
  r.BASIC = new Route(
      '/', undefined,
      isSettingsRefresh2026Enabled ? 'settings20:home' : undefined);
  if (isSettingsRefresh2026Enabled) {
    r.BASIC.section = 'home';
  }

  r.ABOUT = r.BASIC.createSection(
      '/help', 'about', loadTimeData.getString('aboutPageTitle'),
      'cr:chrome-product');

  // Search page.
  r.SEARCH = r.BASIC.createSection(
      '/search', 'search', loadTimeData.getString('searchPageTitle'),
      'settings:search');

  if (!loadTimeData.getBoolean('searchSettingsUpdate')) {
    r.SEARCH_ENGINES = r.SEARCH.createChild('/searchEngines');
  }

  const visibility = pageVisibility || {};

  if (isSettingsRefresh2026Enabled) {
    if (visibility.home !== false) {
      addSharedPeopleRoutes(r, r.BASIC);
    }
  } else if (visibility.people !== false) {
    let peopleIcon = 'settings:person';
    // <if expr="_google_chrome">
    peopleIcon = 'settings-internal:google-g';
    // </if>
    r.PEOPLE = r.BASIC.createSection(
        '/people', 'people', loadTimeData.getString('peoplePageTitle'),
        peopleIcon);
    addSharedPeopleRoutes(r, r.PEOPLE);
  }

  if (visibility.ai !== false && loadTimeData.getBoolean('showAiPage')) {
    r.AI = r.BASIC.createSection(
        '/ai', 'ai', loadTimeData.getString('aiPageTitle'),
        'settings20:button-magic');
    if (loadTimeData.getBoolean('enableAiModeSearchSetting')) {
      r.AI_MODE_SEARCH = r.AI.createChild('/ai/aiModeSearch');
    }
    if (loadTimeData.getBoolean('showComposeControl')) {
      r.OFFER_WRITING_HELP =
          r.AI.createChild('/ai/helpMeWrite', 'settings20:pen-spark');
    }
    const aiSettingsSection = loadTimeData.getString(
        'aiSettingsSection') as AiSettingsSection;
    switch (aiSettingsSection) {
      case AiSettingsSection.NONE:
        break;
      case AiSettingsSection.GEMINI: {
        let geminiIcon = 'settings20:lightbulb-2';
        // <if expr="_google_chrome">
        geminiIcon = getSparkIcon();
        // </if>
        r.GEMINI = r.AI.createChild('/ai/gemini', geminiIcon);
        if (loadTimeData.getBoolean(
                'actorLoginFederatedLoginSupportEnabled')) {
          r.GEMINI_LOGIN = r.GEMINI.createChild('/ai/gemini/login');
        }
        break;
      }
      case AiSettingsSection.GEMINI_ENTERPRISE:
        r.GEMINI_ENTERPRISE =
            r.AI.createChild('/ai/geminienterprise', 'settings20:button-magic');
        break;
      default:
        assertNotReachedCase(aiSettingsSection);
    }
    if (loadTimeData.getBoolean('showAiSuggestionsControl')) {
      r.AI_SUGGESTIONS =
          r.AI.createChild('/ai/suggestions', 'settings20:button-auto');
    }
    if (loadTimeData.getBoolean('showInlineCueMenuControl')) {
      r.INLINE_CUE_MENU =
          r.AI.createChild('/ai/inlineCueMenu', 'settings20:text-select-end');
    }
    if (loadTimeData.getBoolean('showSkillsSettingPage')) {
      r.SKILLS = r.AI.createChild('/ai/skills', 'settings20:flash-on-filled');
    }
    if (loadTimeData.getBoolean('showDictationControl')) {
      r.DICTATION = r.AI.createChild('/ai/dictation', 'privacy:mic');
    }
    // <if expr="_google_chrome">
    if (loadTimeData.getBoolean('showOnDeviceAiAdvancedSettings')) {
      r.ON_DEVICE_AI = r.AI.createChild('/ai/onDeviceAi');
    }
    // </if>
  }

  if (visibility.appearance !== false) {
    r.APPEARANCE = r.BASIC.createSection(
        '/appearance', 'appearance',
        loadTimeData.getString('appearancePageTitle'), 'settings:palette');
    r.FONTS = r.APPEARANCE.createChild('/fonts');
  }

  if (visibility.yourSavedInfo !== false) {
    r.AUTOFILL = r.BASIC.createSection(
        '/autofill', 'yourSavedInfo',
        loadTimeData.getString('autofillPageTitle'),
        'settings:password-manager');

    r.PAYMENTS = r.AUTOFILL.createChild('/payments', 'settings20:credit-card');
    r.CONTACT_INFO =
        r.AUTOFILL.createChild('/contactInfo', 'settings20:location-on');
    r.IDENTITY_DOCS =
        r.AUTOFILL.createChild('/identityDocs', 'settings20:id-card');
    r.TRAVEL = r.AUTOFILL.createChild('/travel', 'settings20:travel');
    if (loadTimeData.getBoolean('offersAndLoyaltyCardsSettingsEnabled')) {
      r.OFFERS_AND_LOYALTY_CARDS = r.AUTOFILL.createChild(
          '/offersAndLoyaltyCards', 'settings20:loyalty-programs');
    }
    if (loadTimeData.getBoolean('shoppingIntegrationEnabled')) {
      r.SHOPPING =
          r.AUTOFILL.createChild('/shopping', 'settings20:shopping-bag');
    }
    if (loadTimeData.getBoolean('showSuggestionsFromGeminiSettings')) {
      let suggestionsIcon = 'settings20:lightbulb';
      // <if expr="_google_chrome">
      suggestionsIcon = getSparkIcon();
      // </if>
      r.SUGGESTIONS_FROM_GEMINI =
          r.AUTOFILL.createChild('/enhancedAutofill', suggestionsIcon);
    }
    // <if expr="is_win or is_macosx">
    r.PASSKEYS = r.AUTOFILL.createChild('/passkeys');
    // </if>
  }

  if (visibility.privacy !== false) {
    r.PRIVACY = r.BASIC.createSection(
        '/privacy', 'privacy', loadTimeData.getString('privacyPageTitle'),
        'settings:security');
    addPrivacyChildRoutes(r);
  }

  // <if expr="not is_chromeos">
  if (!isSettingsRefresh2026Enabled && visibility.defaultBrowser !== false) {
    r.DEFAULT_BROWSER = r.BASIC.createSection(
        '/defaultBrowser', 'defaultBrowser',
        loadTimeData.getString('defaultBrowser'), 'settings:web-asset');
  }
  // </if>

  if (!isSettingsRefresh2026Enabled && visibility.onStartup !== false) {
    r.ON_STARTUP = r.BASIC.createSection(
        '/onStartup', 'onStartup', loadTimeData.getString('onStartup'),
        'settings:power-settings-new');
  }

  // Advanced Routes
  r.ADVANCED = new Route('/advanced');

  if (visibility.languages !== false) {
    r.LANGUAGES = r.ADVANCED.createSection(
        '/languages', 'languages', loadTimeData.getString('languagesPageTitle'),
        'settings:translate');
    // <if expr="not is_chromeos and not is_macosx">
    r.EDIT_DICTIONARY = r.LANGUAGES.createChild('/editDictionary');
    // </if>
  }

  if (!isSettingsRefresh2026Enabled && visibility.downloads !== false) {
    r.DOWNLOADS = r.ADVANCED.createSection(
        '/downloads', 'downloads', loadTimeData.getString('downloadsPageTitle'),
        'settings:download');
  }

  if (visibility.a11y !== false) {
    r.ACCESSIBILITY = r.ADVANCED.createSection(
        '/accessibility', 'a11y', loadTimeData.getString('a11yPageTitle'),
        'settings:accessibility');

    // <if expr="is_linux">
    r.CAPTIONS = r.ACCESSIBILITY.createChild('/captions');
    // </if>
  }

  if (isSettingsRefresh2026Enabled && visibility.general !== false) {
    r.GENERAL = r.ADVANCED.createSection(
        '/general', 'general', loadTimeData.getString('generalPageTitle'),
        'cr:settings-filled');
    r.RESET_DIALOG = r.GENERAL.createChild('/resetProfileSettings');
    r.RESET_DIALOG.isNavigableDialog = true;
    r.TRIGGERED_RESET_DIALOG =
        r.GENERAL.createChild('/triggeredResetProfileSettings');
    r.TRIGGERED_RESET_DIALOG.isNavigableDialog = true;
  }

  // <if expr="not is_chromeos">
  if (!isSettingsRefresh2026Enabled && visibility.system !== false) {
    r.SYSTEM = r.ADVANCED.createSection(
        '/system', 'system', loadTimeData.getString('systemPageTitle'),
        'settings:build');
  }
  // </if>

  if (!isSettingsRefresh2026Enabled && visibility.reset !== false) {
    r.RESET = r.ADVANCED.createSection(
        '/reset', 'reset', loadTimeData.getString('resetPageTitle'),
        'settings:restart-alt');
    r.RESET_DIALOG = r.RESET.createChild('/resetProfileSettings');
    r.RESET_DIALOG.isNavigableDialog = true;
    r.TRIGGERED_RESET_DIALOG =
        r.RESET.createChild('/triggeredResetProfileSettings');
    r.TRIGGERED_RESET_DIALOG.isNavigableDialog = true;
  }

  if (visibility.performance !== false) {
    r.PERFORMANCE = r.BASIC.createSection(
        '/performance', 'performance',
        loadTimeData.getString('performancePageTitle'), 'settings:speed');
  }

  return r as unknown as SettingsRoutes;
}

/**
 * @return A router with the browser settings routes.
 */
export function buildRouter(): Router {
  return new Router(createRoutes());
}

export function resetRouterForTesting(router: Router = buildRouter()) {
  Router.resetInstanceForTesting(router);

  // Update the exported `routes` variable, otherwise it will be holding stale
  // routes from the previous singleton instance.
  routes = Router.getInstance().getRoutes();
}

Router.setInstance(buildRouter());
window.addEventListener('popstate', function() {
  // On pop state, do not push the state onto the window.history again.
  const routerInstance = Router.getInstance();
  routerInstance.setCurrentRoute(
      routerInstance.getRouteForPath(window.location.pathname) ||
          routerInstance.getRoutes().BASIC,
      new URLSearchParams(window.location.search), true);
});

export let routes: SettingsRoutes = Router.getInstance().getRoutes();

// Returns the "effective" route when at chrome://settings/.
export function getTopLevelRoute(): Route {
  if (!loadTimeData.getBoolean('isGuest')) {
    return loadTimeData.getString('settingsRefresh2026') !== '' ? routes.BASIC :
                                                                  routes.PEOPLE;
  }

  let guestTopLevelRoute = routes.SEARCH;
  // <if expr="is_chromeos">
  guestTopLevelRoute = routes.PRIVACY;
  // </if>

  return guestTopLevelRoute;
}
