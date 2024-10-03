// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Root element of the OOBE UI Debugger.
 */

import {MessageType, ProblemType} from '//resources/ash/common/quick_unlock/setup_pin_keyboard.js';
import {$} from '//resources/ash/common/util.js';
import {assert, assertInstanceof} from '//resources/js/assert.js';
import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';

import {Oobe} from '../cr_ui.js';
import {loadTimeData} from '../i18n_setup.js';

function createQuickStartQR(): boolean[] {
  // Fake data extracted from the real flow.
  const qrDataStr =
      '1111111000011000010010000001101111111100000100111100001001010011100100000110111010111011101001111010110010111011011101010100011110010001110001011101101110101101101100110111100010101110110000010100011001010100001010010000011111111010101010101010101010101111111000000001101010010001111101010000000010111110011000100101111000101011111001001100011111111111111111100111101010101100110001000001101110111110101101110001100111101101001111010100101010010110111010110000010001110111011110111110100000110010111110101010000010001000100111000111111100110001010111110111111010100000101000011001000100110001111010111010100110011110101011100011111111101111011010000110001001001011100000111100010101100000100111100111011100110011100000100001110101111101000110011011110010111100000001100110110000111000100010000000111110000100001010010100101101000101100100010111011100111001001011010111001111100010001100001101001101010010010001110110011000100111100000110011111101111001001110010010110111011001000010101001110110110111010100001100110100111001011010001011101100110011010001101111111011111010100000000101011111000111111011000110101111111001110111010000100100101010111100000101001001010100100000010001101110111010110000010000101001011111101011011101011001111011010011111111011011101110101100011110000000001010000111110000010000011110000110100001010100011111111011011110100010001111001010111';
  // Screen expects an array of booleans representing the pixels.
  const qrData = [];
  for (const pixel of qrDataStr) {
    qrData.push(pixel === '1' ? true : false);
  }
  return qrData;
}

function createPerksData() {
  return [
    {
      perkId: 'google_one',
      title: 'Get 100G of cloud storage',
      subtitle:
          'Your Chromebook comes with 100GB of cloud storage. Enjoy plenty of space for all your files and photos with 12 months of Google One at no cost. Terms apply.',
      iconUrl:
          'https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/google_one_icon.svg',
      illustrationUrl:
          'https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/google_one_illustration.svg',
      illustrationWidth: '406px',
      illustrationHeight: '342px',
      primaryButtonLabel: 'Get perk after setup',
      secondaryButtonLabel: 'Not interested',
    },
    {
      perkId: 'youtube_premuim',
      title: 'Get 3 months of YouTube Premium on us',
      subtitle:
          'Get 3 months of YouTube Premium free of charge with your Chromebook and enjoy your favorite videos and music, ad-free.',
      iconUrl:
          'https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/youtube_icon.svg',
      illustrationUrl:
          'https://www.gstatic.com/chromeos-oobe-eng/oobe-perks/youtube_illustration.svg',
      illustrationWidth: '400px',
      illustrationHeight: '280px',
      primaryButtonLabel: 'Get perk after setup',
      secondaryButtonLabel: 'Not interested',
    },
  ];
}


function createCategoriesAppsData() {
  const data = [
    {
      name: 'categorie_1',
      apps: [
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/dVsv8Hc4TOUeLFAahxR8KANg22W9dj2jBsTW1VHv3CV-5NCZjP9D9i2j5IpfVx2NTB8',
          name: 'Pinterest',
          subname: 'Music streaming',
          package_name: 'Pinterest',
          selected: false,
        },
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/kMofEFLjobZy_bCuaiDogzBcUT-dz3BBbOrIEjJ-hqOabjK8ieuevGe6wlTD15QzOqw',
          name: 'WhatsApp Messenger',
          subname: 'Office software',
          package_name: 'Pinterest',
          selected: false,
        },
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/bYtqbOcTYOlgc6gqZ2rwb8lptHuwlNE75zYJu6Bn076-hTmvd96HH-6v7S0YUAAJXoJN',
          name: 'Clash Royale',
          subname: 'Messaging',
          package_name: 'Pinterest',
          selected: false,
        },
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/dVsv8Hc4TOUeLFAahxR8KANg22W9dj2jBsTW1VHv3CV-5NCZjP9D9i2j5IpfVx2NTB8',
          name: 'Zoom',
          subname: 'Cloud gaming',
          package_name: 'Pinterest',
          selected: false,
        },
      ],
    },
    {
      name: 'categorie_23',
      apps: [
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/dVsv8Hc4TOUeLFAahxR8KANg22W9dj2jBsTW1VHv3CV-5NCZjP9D9i2j5IpfVx2NTB8',
          name: 'Pinterest',
          subname: 'Music streaming',
          package_name: 'Pinterest',
          selected: false,
        },
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/dVsv8Hc4TOUeLFAahxR8KANg22W9dj2jBsTW1VHv3CV-5NCZjP9D9i2j5IpfVx2NTB8',
          name: 'WhatsApp Messenger',
          subname: 'Office software',
          package_name: 'Pinterest',
          selected: false,
        },
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/dVsv8Hc4TOUeLFAahxR8KANg22W9dj2jBsTW1VHv3CV-5NCZjP9D9i2j5IpfVx2NTB8',
          name: 'Clash Royale',
          subname: 'Messaging',
          package_name: 'Pinterest',
          selected: false,
        },
        {
          AppId: 'screenID1',
          icon:
              'https://lh3.googleusercontent.com/dVsv8Hc4TOUeLFAahxR8KANg22W9dj2jBsTW1VHv3CV-5NCZjP9D9i2j5IpfVx2NTB8',
          name: 'Zoom',
          subname: 'Cloud gaming',
          package_name: 'Pinterest',
          selected: false,
        },
      ],
    },
  ];
  return data;
}

function createCategoriesData() {
  const data = {
    categories: [
      {
        categoryId: 'oobe_business',
        icon: 'https://meltingpot.googleusercontent.com/oobe/business.svg',
        title: 'Small Business',
        subtitle: 'Small business essentials',
        selected: false,
      },
      {
        categoryId: 'oobe_entertainment',
        icon: 'https://meltingpot.googleusercontent.com/oobe/entertainment.svg',
        title: 'Entertainment',
        subtitle: 'Media, music, video streaming',
        selected: false,
      },
      {
        categoryId: 'oobe_communication',
        icon: 'https://meltingpot.googleusercontent.com/oobe/communication.svg',
        title: 'Communication',
        subtitle: 'Messaging, video chat, social media',
        selected: false,
      },
      {
        categoryId: 'oobe_creativity',
        icon: 'https://meltingpot.googleusercontent.com/oobe/productivity.svg',
        title: 'Creativity',
        subtitle: 'Drawing, design and media editing',
        selected: false,
      },
      {
        categoryId: 'oobe_productivity',
        icon: 'https://meltingpot.googleusercontent.com/oobe/creativity.svg',
        title: 'Productivity',
        subtitle: 'Home office, productivity work',
        selected: false,
      },
      {
        categoryId: 'oobe_gaming',
        icon: 'https://meltingpot.googleusercontent.com/oobe/gaming.svg',
        title: 'Gaming',
        subtitle: 'Action, adventure, strategy, puzzle games and more',
        selected: false,
      },
    ],
  };
  return data;
}

const DEBUG_BUTTON_STYLE = `
    height:20px;
    width:120px;
    position: absolute;
    top: 0;
    left: calc(50% - 60px);
    background-color: red;
    color: white;
    z-index: 10001;
    text-align: center;`;

const DEBUG_OVERLAY_STYLE = `
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    bottom : 0;
    background-color: rgba(0, 0, 255, 0.90);
    color: white;
    z-index: 10000;
    padding: 20px;
    display: flex;
    overflow: scroll;
    flex-direction: column;`;

const TOOL_PANEL_STYLE = `
    direction: ltr;
    display: flex;
    flex-wrap: wrap;
    flex-direction: row;`;

const TOOL_BUTTON_STYLE = `
    border: 2px solid white;
    padding: 3px 10px;
    margin: 3px 4px;
    text-align: center;
    vertical-align: middle;
    font-weight: bold;`;

const TOOL_BUTTON_SELECTED_STYLE = `
    border-width: 4px !important;
    padding: 2px 9px !important;
    margin: 2px 3px !important;`;

// White.
const SCREEN_BUTTON_STYLE_NORMAL = `
    border-color: #fff !important;
    color: #fff;`;
// Orange-tinted.
const SCREEN_BUTTON_STYLE_ERROR = `
    border-color: #f80 !important;
    color: #f80`;
// Green-tinted.
const SCREEN_BUTTON_STYLE_OTHER = `
    border-color: #afa !important;
    color: #afa`;
// Pink-tinted.
const SCREEN_BUTTON_STYLE_UNKNOWN = `
    border-color: #faa !important;
    color: #faa`;

const BUTTON_COMMAND_DELAY: number = 100;
const SCREEN_LOADING_DELAY: number = 500;
const SCREENSHOT_CAPTURE_DELAY: number = 200;

/**
 * Indicates if screen is present in usual user flow, represents some error
 * state or is shown in some other cases. See KNOWN_SCREENS for more details.
 */
const enum ScreenKind {
  NORMAL = 'normal',
  ERROR = 'error',
  OTHER = 'other',
  UNKNOWN = 'unknown',
}

interface ScreenDefType {
  id: string;
  kind: ScreenKind;
  suffix?: string;
  index?: number;
  skipScreenshots?: boolean;
  defaultState?: string;
  data?: any;
  handledSteps?: string;
  states?: StateDefType[];
  stateMap?: Record<string, any>;
}

interface StateDefType {
  id: string;
  data?: any;
  trigger?: (screen: HTMLElement|null) => void;
}

/**
 * List of possible screens.
 * Attributes:
 *    id - ID of the screen passed to login.createScreen
 *    kind - Used for button coloring, indicates if screen is present in
 *           normal flow (via debug-button-<kind> classes).
 *    suffix - extra text to display on button, gives some insight
 *             on context in which screen is shown (e.g. Enterprise-specific
 *             screens).
 *    data - extra data passed to DisplayManager.showScreen.
 */
const KNOWN_SCREENS: ScreenDefType[] = [
  {
    // Device without mouse/keyboard.
    id: 'hid-detection',
    kind: ScreenKind.NORMAL,
    states: [
      {
        id: 'searching',
        trigger: (screen: any) => {
          screen.setKeyboardDeviceName('Some Keyboard');
          screen.setPointingDeviceName('Some Mouse');
          screen.setKeyboardState('searching');
          screen.setMouseState('searching');
          screen.setTouchscreenDetectedState(false);
          screen.setContinueButtonEnabled(false);
          screen.setPinDialogVisible(false);
        },
      },
      {
        id: 'connected',
        trigger: (screen: any) => {
          screen.setKeyboardDeviceName('Some Keyboard');
          screen.setPointingDeviceName('Some Mouse');
          screen.setKeyboardState('connected');
          screen.setMouseState('paired');
          screen.setTouchscreenDetectedState(false);
          screen.setContinueButtonEnabled(true);
          screen.setPinDialogVisible(false);
        },
      },
      {
        id: 'pairing-pin',
        trigger: (screen: any) => {
          screen.setKeyboardDeviceName('Some Keyboard');
          screen.setPointingDeviceName('Some Mouse');
          screen.setKeyboardState('pairing');
          screen.setMouseState('pairing');
          screen.setTouchscreenDetectedState(false);
          screen.setContinueButtonEnabled(false);
          screen.setPinDialogVisible(true);
          screen.setNumKeysEnteredPinCode(1);
        },
      },
      {
        id: 'touchscreen-detected',
        trigger: (screen: any) => {
          screen.setKeyboardDeviceName('Some Keyboard');
          screen.setPointingDeviceName('Some Mouse');
          screen.setKeyboardState('searching');
          screen.setMouseState('searching');
          screen.setTouchscreenDetectedState(true);
          screen.setContinueButtonEnabled(true);
          screen.setPinDialogVisible(false);
        },
      },

    ],
  },
  {
    // Welcome screen.
    id: 'connect',
    kind: ScreenKind.NORMAL,
    data: {
      isDeveloperMode: true,
    },
    states: [
      {
        id: 'default',
        trigger: (screen: any) => {
          screen.closeTimezoneSection_();
        },
      },
      {
        id: 'acessibility',
        trigger: (screen: any) => {
          screen.onWelcomeAccessibilityButtonClicked_();
        },
      },
      {
        id: 'languages',
        trigger: (screen: any) => {
          screen.onWelcomeSelectLanguageButtonClicked_();
        },
      },
      {
        // Available on Chromebox for meetings.
        id: 'timezone',
        trigger: (screen: any) => {
          screen.onWelcomeTimezoneButtonClicked_();
        },
      },
      {
        // Advanced options, shown by long press on "Welcome!" title.
        id: 'adv-options',
        trigger: (screen: any) => {
          screen.onWelcomeLaunchAdvancedOptions_();
        },
      },
    ],
  },
  {
    id: 'os-install',
    kind: ScreenKind.OTHER,
    handledSteps: 'success',
    states: [
      {
        id: 'success',
        trigger: (screen: any) => {
          screen.updateCountdownString(
              'Your device will shut down in 60 seconds. Remove the USB' +
              ' before turning your device back on. Then you can start' +
              ' using ChromeOS Flex.');
          screen.showStep('success');
        },
      },
    ],
  },
  {
    id: 'os-trial',
    kind: ScreenKind.OTHER,
  },
  {
    id: 'debugging',
    kind: ScreenKind.OTHER,
  },
  {
    id: 'demo-preferences',
    kind: ScreenKind.OTHER,
    suffix: 'demo',
  },
  {
    id: 'network-selection',
    kind: ScreenKind.NORMAL,
    states: [
      {
        id: 'no-error',
      },
      {
        id: 'error',
        trigger: (screen: any) => {
          screen.setError(
              'ChromeOS was unable to connect to Public Wifi. ' +
              'Please select another network or try again.');
        },
      },
    ],
  },
  {
    id: 'demo-setup',
    kind: ScreenKind.OTHER,
    suffix: 'demo',
    handledSteps: 'progress,error',
    states: [
      {
        id: 'download-resources',
        trigger: (screen: any) => {
          screen.setCurrentSetupStep('downloadResources');
        },
      },
      {
        id: 'enrollment',
        trigger: (screen: any) => {
          screen.setCurrentSetupStep('enrollment');
        },
      },
      {
        id: 'done',
        trigger: (screen: any) => {
          screen.setCurrentSetupStep('complete');
          screen.onSetupSucceeded();
        },
      },
      {
        id: 'error',
        trigger: (screen: any) => {
          screen.onSetupFailed('Some error message', true);
        },
      },
    ],
  },
  {
    id: 'oobe-update',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'consumer-update',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'perks-discovery',
    kind: ScreenKind.NORMAL,
    handledSteps: 'overview',
    states: [
      {
        id: 'overview',
        trigger: (screen: any) => {
          screen.setPerksData(createPerksData());
          screen.setOverviewStep();
        },
      },
    ],
  },
  {
    id: 'auto-enrollment-check',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'device-disabled',
    kind: ScreenKind.ERROR,
    suffix: 'E',
    // TODO: remove this once all screen switching logic is
    // moved to C++ side.
    skipScreenshots: true,
    states: [
      {
        // No enrollment domain specified
        id: 'no-domain',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            serial: '1234567890',
            domain: '',
            message: 'Some custom message provided by org admin.',
          });
        },
      },
      {
        // Enrollment domain was specified
        id: 'has-domain',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            serial: '1234567890',
            domain: 'example.com',
            message: 'Please return this device to the techstop.',
          });
        },
      },
    ],
  },
  {
    id: 'user-creation',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'add-child',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'enterprise-enrollment',
    kind: ScreenKind.NORMAL,
    handledSteps: 'error',
    suffix: 'E',
    data: {
      gaiaPath: 'embedded/setup/v2/chromeos',
      gaiaUrl: 'https://accounts.google.com/',
    },
    states: [
      {
        id: 'error',
        trigger: (screen: any) => {
          screen.showError('Some error message', true);
        },
      },
    ],
  },
  {
    id: 'family-link-notice',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'packaged-license',
    kind: ScreenKind.NORMAL,
    suffix: 'E',
  },
  // Login screen starts here.
  {
    id: 'error-message',
    kind: ScreenKind.ERROR,
    states: [
      {
        // Show offline error during signin
        id: 'signin-offline-error',
        trigger: (screen: any) => {
          screen.setUiState(2);     // signin
          screen.setErrorState(2);  // offline
          screen.allowGuestSignin(true);
          screen.allowOfflineLogin(true);
        },
      },
    ],
  },
  {
    id: 'update-required',
    kind: ScreenKind.OTHER,
    suffix: 'E',
    handledSteps: 'update-required-message,update-process,eol',
    states: [
      {
        id: 'initial',
        trigger: (screen: any) => {
          screen.setUIState(0);
          screen.setEnterpriseAndDeviceName('example.com', 'Chromebook');
        },
      },
      {
        id: 'checking-for-update',
        trigger: (screen: any) => {
          screen.setUIState(1);
          screen.setUpdateProgressUnavailable(true);
          screen.setEstimatedTimeLeftVisible(false);
          screen.setUpdateProgressValue(0);
          screen.setUpdateProgressMessage('Checking for update');
        },
      },
      {
        id: 'in-progress',
        trigger: (screen: any) => {
          screen.setUIState(1);
          screen.setUpdateProgressUnavailable(false);
          screen.setEstimatedTimeLeftVisible(true);
          screen.setEstimatedTimeLeft(114);
          screen.setUpdateProgressValue(33);
          screen.setUpdateProgressMessage('33 percent done');
        },
      },
      {
        id: 'eol',
        trigger: (screen: any) => {
          screen.setUIState(5);
          screen.setEolMessage(
              'Message from admin: please return device somewhere.');
        },
      },
    ],
  },
  {
    id: 'app-launch-splash',
    kind: ScreenKind.OTHER,
    suffix: 'kiosk',
    data: {
      appInfo: {
        name: 'Application Name',
        url: 'http://example.com/someApplication/VeryLongURL',
        iconURL: 'chrome://theme/IDR_LOGO_GOOGLE_COLOR_90',
      },
      shortcutEnabled: true,
    },
  },
  {
    id: 'wrong-hwid',
    kind: ScreenKind.ERROR,
  },
  {
    id: 'tpm-error-message',
    kind: ScreenKind.ERROR,
  },
  {
    id: 'install-attributes-error-message',
    kind: ScreenKind.ERROR,
  },
  {
    id: 'signin-fatal-error',
    kind: ScreenKind.ERROR,
    states: [
      {
        id: 'SCRAPED_PASSWORD_VERIFICATION_FAILURE',
        data: {
          errorState: 1,
        },
      },
      {
        id: 'INSECURE_CONTENT_BLOCKED',
        data: {
          errorState: 2,
          url: 'http://example.url/',
        },
      },
      {
        id: 'MISSING_GAIA_INFO',
        data: {
          errorState: 3,
        },
      },
      {
        id: 'CRYPTOHOME_ERROR',
        data: {
          errorState: 4,
          errorText:
              'Sorry, your password could not be verified. Please try again',
          keyboardHint: 'Check your keyboard layout and try again',
          details: 'Could not mount cryptohome.',
          helpLinkText: 'Learn more',
        },
      },
    ],
  },
  {
    id: 'smart-privacy-protection',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'split-modifier-keyboard-info',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'theme-selection',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'reset',
    kind: ScreenKind.OTHER,
    states: [
      {
        id: 'restart-required',
        trigger: (screen: any) => {
          screen.reset();
          screen.setScreenState(0);
        },
      },
      {
        id: 'revert-promise',
        trigger: (screen: any) => {
          screen.reset();
          screen.setScreenState(1);
        },
      },
      {
        id: 'powerwash-proposal',
        trigger: (screen: any) => {
          screen.reset();
          screen.setScreenState(2);
        },
      },
      {
        id: 'powerwash-rollback',
        trigger: (screen: any) => {
          screen.reset();
          screen.setScreenState(2);
          screen.setIsRollbackAvailable(true);
          screen.setIsRollbackRequested(true);
          screen.setIsTpmFirmwareUpdateAvailable(true);
        },
      },
      {
        id: 'powerwash-confirmation',
        trigger: (screen: any) => {
          screen.reset();
          screen.setShouldShowConfirmationDialog(true);
        },
      },
      {
        id: 'rollback-error',
        trigger: (screen: any) => {
          screen.reset();
          screen.setScreenState(3);
        },
      },
    ],
  },
  {
    // Shown instead of sign-in screen, triggered from Crostini.
    id: 'adb-sideloading',
    kind: ScreenKind.OTHER,
  },
  {
    // Customer kiosk feature.
    id: 'kiosk-enable',
    kind: ScreenKind.OTHER,
  },
  {
    id: 'gaia-signin',
    kind: ScreenKind.NORMAL,
    handledSteps: 'online-gaia,enrollment-nudge',
    states: [
      {
        id: 'online-gaia',
        trigger: (screen: any) => {
          screen.loadAuthenticator({
            chromeType: 'chromedevice',
            enterpriseManagedDevice: false,
            gaiaPath: 'embedded/setup/v2/chromeos',
            gaiaUrl: 'https://accounts.google.com/',
            hl: loadTimeData.getString('app_locale'),
          });
        },
      },
      {
        id: 'enrollment-nudge',
        trigger: (screen: any) => {
          screen.showEnrollmentNudge('example.com');
        },
      },
    ],
  },
  {
    id: 'gaia-info',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'offline-login',
    kind: ScreenKind.NORMAL,
    states: [
      {
        id: 'default',
        trigger: (screen: any) => {
          screen.onBeforeShow({});
        },
      },
      {
        // kAccountsPrefLoginScreenDomainAutoComplete value is set
        id: 'offline-gaia-domain',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            emailDomain: 'somedomain.com',
          });
        },
      },
      {
        // Device is enterprise-managed.
        id: 'offline-gaia-enterprise',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            enterpriseDomainManager: 'example.com',
          });
        },
      },
      {
        // Password and email mismatch error message.
        id: 'offline-login-password-mismatch',
        trigger: (screen: any) => {
          screen.setEmailForTest('someuser@gmail.com');
          screen.proceedToPasswordPage();
          screen.showPasswordMismatchMessage();
        },
      },
    ],
  },
  {
    // Failure during SAML flow.
    id: 'fatal-error',
    kind: ScreenKind.ERROR,
    suffix: 'SAML',
    states: [
      {
        id: 'default',
        trigger: (screen: any) => {
          screen.show(
              'Sign-in failed because your password could not be ' +
                  'verified. Please contact your administrator or try again.',
              'Try again');
        },
      },
    ],
  },
  {
    id: 'ad-password-change',
    kind: ScreenKind.OTHER,
    handledSteps: 'password',
    states: [
      {
        // No error
        id: 'no-error',
        data: {
          username: 'username',
          error: 0,
        },
      },
      {
        // First error
        id: 'error-1',
        data: {
          username: 'username',
          error: 1,
        },
      },
      {
        // Second error
        id: 'error-2',
        data: {
          username: 'username',
          error: 2,
        },
      },
      {
        // Error dialog
        id: 'error-dialog',
        trigger: (screen: any) => {
          const error = 'Some error text';
          screen.showErrorDialog(error);
        },
      },
    ],
  },
  {
    id: 'encryption-migration',
    kind: ScreenKind.OTHER,
    handledSteps: 'ready,migrating,not-enough-space',
    states: [
      {
        id: 'ready',
        trigger: (screen: any) => {
          screen.setUIState(1);
          screen.setBatteryState(100, true, true);
          screen.setNecessaryBatteryPercent(40);
        },
      },
      {
        id: 'ready-low-battery',
        trigger: (screen: any) => {
          screen.setUIState(1);
          screen.setBatteryState(11, false, false);
          screen.setNecessaryBatteryPercent(40);
        },
      },
      {
        id: 'migrating',
        trigger: (screen: any) => {
          screen.setUIState(2);
          screen.setMigrationProgress(0.37);
        },
      },
      {
        id: 'not-enough-space',
        trigger: (screen: any) => {
          screen.setUIState(4);
          screen.setSpaceInfoInString(
              '1 GB' /* availableSpaceSize */, '2 GB' /* necessarySpaceSize */);
        },
      },
    ],
  },
  {
    id: 'apply-online-password',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'local-data-loss-warning',
    kind: ScreenKind.NORMAL,
    states: [
      {
        id: 'non-owner',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            isOwner: false,
            email: 'someone@example.com',
          });
        },
      },
      {
        id: 'owner',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            isOwner: true,
            email: 'someone@example.com',
          });
        },
      },
    ],
  },
  {
    id: 'enter-old-password',
    kind: ScreenKind.NORMAL,
    states: [
      {
        id: 'no-error',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            passwordInvalid: false,
          });
        },
      },
      {
        id: 'wrong-password',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            passwordInvalid: true,
          });
        },
      },
    ],
  },
  {
    id: 'local-password-setup',
    kind: ScreenKind.NORMAL,
    states: [
      {
        // Forced password setup
        id: 'forced',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            showBackButton: false,
            isRecoveryFlow: false,
          });
        },
      },
      {
        // Forced password setup
        id: 'forced-recovery',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            showBackButton: false,
            isRecoveryFlow: true,
          });
        },
      },
      {
        // Forced password setup
        id: 'optional',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            showBackButton: true,
            isRecoveryFlow: false,
          });
        },
      },
    ],
  },
  {
    id: 'osauth-error',
    kind: ScreenKind.ERROR,
  },
  {
    id: 'factor-setup-success',
    kind: ScreenKind.NORMAL,
    states: [
      {
        id: 'local-set',
        data: {
          modifiedFactors: 'local',
          changeMode: 'set',
        },
      },
      {
        id: 'local-update',
        data: {
          modifiedFactors: 'local',
          changeMode: 'update',
        },
      },
    ],
  },
  {
    id: 'saml-confirm-password',
    kind: ScreenKind.OTHER,
    suffix: 'SAML',
    handledSteps: 'password',
    states: [
      {
        // Password was scraped
        id: 'scraped',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            email: 'someone@example.com',
            manualPasswordInput: false,
          });
          screen.showPasswordStep(false);
        },
      },
      {
        // Password was scraped
        id: 'scraped-retry',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            email: 'someone@example.com',
            manualPasswordInput: false,
          });
          screen.showPasswordStep(true);
        },
      },
      {
        // No password was scraped
        id: 'manual',
        trigger: (screen: any) => {
          screen.onBeforeShow({
            email: 'someone@example.com',
            manualPasswordInput: true,
          });
          screen.showPasswordStep(false);
        },
      },
    ],
  },
  {
    id: 'management-transition',
    kind: ScreenKind.OTHER,
    handledSteps: 'progress,error',
    states: [
      {
        id: 'add-supervision',
        trigger: (screen: any) => {
          screen.setArcTransition(2);
          screen.setUIStep('progress');
        },
      },
      {
        id: 'remove-supervision',
        trigger: (screen: any) => {
          screen.setArcTransition(1);
          screen.setUIStep('progress');
        },
      },
      {
        id: 'add-management',
        trigger: (screen: any) => {
          screen.setArcTransition(3);
          screen.setManagementEntity('example.com');
          screen.setUIStep('progress');
        },
      },
      {
        id: 'add-management-unknown-admin',
        trigger: (screen: any) => {
          screen.setArcTransition(3);
          screen.setManagementEntity('');
          screen.setUIStep('progress');
        },
      },
      {
        id: 'error-supervision',
        trigger: (screen: any) => {
          screen.setArcTransition(1);
          screen.setUIStep('error');
        },
      },
      {
        id: 'error-management',
        trigger: (screen: any) => {
          screen.setArcTransition(3);
          screen.setUIStep('error');
        },
      },
    ],
  },
  {
    id: 'terms-of-service',
    kind: ScreenKind.NORMAL,
    handledSteps: 'loading,loaded,error',
    states: [
      {
        id: 'loading',
        trigger: (screen: any) => {
          screen.onBeforeShow({manager: 'TestCompany'});
          screen.setUIStep('loading');
        },
      },
      {
        id: 'loaded',
        trigger: (screen: any) => {
          screen.onBeforeShow({manager: 'TestCompany'});
          screen.setTermsOfService('TOS BEGIN\nThese are the terms\nTOS END');
        },
      },
      {
        id: 'error',
        trigger: (screen: any) => {
          screen.setTermsOfServiceLoadError();
        },
      },
    ],
  },
  {
    id: 'sync-consent',
    kind: ScreenKind.NORMAL,
    handledSteps: 'ash-sync,lacros-overview',
    states: [
      {
        id: 'ash-sync',
        data: {
          isChildAccount: false,
          isArcRestricted: false,
        },
        trigger: (screen: any) => {
          screen.setIsMinorMode(false);
          screen.showLoadedStep(/*os_sync_lacros=*/ false);
        },
      },
      {
        id: 'ash-sync-minor-mode',
        data: {
          isChildAccount: true,
          isArcRestricted: false,
        },
        trigger: (screen: any) => {
          screen.setIsMinorMode(true);
          screen.showLoadedStep(/*os_sync_lacros=*/ false);
        },
      },
      {
        id: 'ash-sync-arc-restricted',
        data: {
          isChildAccount: false,
          isArcRestricted: true,
        },
        trigger: (screen: any) => {
          screen.setIsMinorMode(false);
          screen.showLoadedStep(/*os_sync_lacros=*/ false);
        },
      },
      {
        id: 'lacros-overview',
        data: {
          isChildAccount: false,
          isArcRestricted: false,
        },
        trigger: (screen: any) => {
          screen.setIsMinorMode(false);
          screen.showLoadedStep(/*os_sync_lacros=*/ true);
        },
      },
      {
        id: 'lacros-overview-minor',
        data: {
          isChildAccount: true,
          isArcRestricted: false,
        },
        trigger: (screen: any) => {
          screen.setIsMinorMode(true);
          screen.showLoadedStep(/*os_sync_lacros=*/ true);
        },
      },
    ],
  },
  {
    id: 'consolidated-consent',
    kind: ScreenKind.NORMAL,
    handledSteps:
        'loaded,loading,play-load-error,google-eula,cros-eula,arc,privacy',
    // TODO(b/260014420): Use localized URLs for eulaUrl and additionalTosUrl.
    states: [
      {
        id: 'regular-owner',
        trigger: (screen: any) => {
          screen.setUsageOptinHidden(false);
        },
        data: {
          isArcEnabled: true,
          isDemo: false,
          isChildAccount: false,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'regular',
        trigger: (screen: any) => {
          screen.setUsageOptinHidden(true);
        },
        data: {
          isArcEnabled: true,
          isDemo: false,
          isChildAccount: false,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'regular-recovery',
        trigger: (screen: any) => {
          screen.setUsageOptinHidden(true);
        },
        data: {
          isArcEnabled: true,
          isDemo: false,
          isChildAccount: false,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: true,
          recoveryOptionDefault: true,
        },
      },
      {
        id: 'child-owner',
        trigger: (screen: any) => {
          screen.setUsageOptinHidden(false);
        },
        data: {
          isArcEnabled: true,
          isDemo: false,
          isChildAccount: true,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'child',
        trigger: (screen: any) => {
          screen.setUsageOptinHidden(true);
        },
        data: {
          isArcEnabled: true,
          isDemo: false,
          isChildAccount: true,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'demo-mode',
        data: {
          isArcEnabled: true,
          isDemo: true,
          isChildAccount: false,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'arc-disabled-owner',
        trigger: (screen: any) => {
          screen.setUsageOptinHidden(false);
        },
        data: {
          isArcEnabled: false,
          isDemo: false,
          isChildAccount: false,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'arc-disabled',
        trigger: (screen: any) => {
          screen.setUsageOptinHidden(true);
        },
        data: {
          isArcEnabled: false,
          isDemo: false,
          isChildAccount: false,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'managed-account',
        trigger: (screen: any) => {
          screen.setBackupMode(true, true);
          screen.setLocationMode(false, true);
          screen.setUsageOptinHidden(true);
        },
        data: {
          isArcEnabled: true,
          isDemo: false,
          isChildAccount: false,
          isTosHidden: true,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },
      {
        id: 'play-load-error',
        trigger: (screen: any) => {
          screen.setUIStep('play-load-error');
        },
        data: {
          isArcEnabled: true,
          isDemo: false,
          isChildAccount: false,
          isTosHidden: false,
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          arcTosUrl: 'https://play.google/play-terms/embedded/',
          privacyPolicyUrl: 'https://policies.google.com/privacy/embedded',
          showRecoveryOption: false,
          recoveryOptionDefault: false,
        },
      },

    ],
  },
  {
    id: 'cryptohome-recovery-setup',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'guest-tos',
    kind: ScreenKind.NORMAL,
    handledSteps: 'loading,overview,google-eula,cros-eula',
    // TODO(b/260014420): Use localized URLs for googleEulaURL and crosEulaURL.
    states: [
      {
        id: 'overview',
        trigger: (screen: any) => {
          screen.setUIStep('overview');
        },
        data: {
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
        },
      },
      {
        id: 'loading',
        trigger: (screen: any) => {
          screen.setUIStep('loading');
        },
        data: {
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
        },
      },
      {
        id: 'google-eula',
        trigger: (screen: any) => {
          screen.setUIStep('google-eula');
        },
        data: {
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
        },
      },
      {
        id: 'cros-eula',
        trigger: (screen: any) => {
          screen.setUIStep('cros-eula');
        },
        data: {
          googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
          crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
        },
      },
    ],
  },
  {
    id: 'hw-data-collection',
    kind: ScreenKind.OTHER,
  },
  {
    id: 'local-state-error',
    kind: ScreenKind.ERROR,
  },
  {
    id: 'fingerprint-setup',
    kind: ScreenKind.NORMAL,
    defaultState: 'default',
    handledSteps: 'progress',
    states: [
      {
        id: 'progress-0',
        trigger: (screen: any) => {
          screen.onEnrollScanDone(0 /* success */, false, 0);
          screen.onEnrollScanDone(0 /* success */, false, 0);
        },
      },
      {
        id: 'error-immobile',
        trigger: (screen: any) => {
          screen.onEnrollScanDone(0 /* success */, false, 0);
          screen.onEnrollScanDone(6, false, 30);
        },
      },
      {
        id: 'progress-60',
        trigger: (screen: any) => {
          screen.onEnrollScanDone(0 /* success */, false, 0);
          screen.onEnrollScanDone(0 /* success */, false, 60);
        },
      },
      {
        id: 'progress-100',
        trigger: (screen: any) => {
          screen.onEnrollScanDone(0 /* success */, true, 100);
        },
      },
    ],
  },
  {
    id: 'pin-setup',
    kind: ScreenKind.NORMAL,
    states: [
      {
        id: 'clear-error',
        trigger: (screen: any) => {
          (screen.$).pinKeyboard.hideProblem_();
        },
      },
      {
        id: 'error-warning',
        trigger: (screen: any) => {
          (screen.$).pinKeyboard.showProblem_(
              MessageType.TOO_WEAK, ProblemType.WARNING);
        },
      },
      {
        id: 'error-error',
        trigger: (screen: any) => {
          (screen.$).pinKeyboard.showProblem_(
              MessageType.TOO_LONG, ProblemType.ERROR);
        },
      },
      {
        id: 'pin-as-main-factor',
        data: {
          authToken: '',
          isChildAccount: false,
          hasLoginSupport: true,
          usingPinAsMainSignInFactor: true,
        },
      },
      {
        id: 'pin-for-unlock-only',
        data: {
          authToken: '',
          isChildAccount: false,
          hasLoginSupport: false,
          usingPinAsMainSignInFactor: false,
        },
      },
      {
        id: 'pin-default',
        data: {
          authToken: '',
          isChildAccount: false,
          hasLoginSupport: true,
          usingPinAsMainSignInFactor: false,
        },
      },
    ],
  },
  {
    id: 'password-selection',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'arc-vm-data-migration',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'recommend-apps',
    kind: ScreenKind.NORMAL,
    handledSteps: 'list',
    // Known issue: reset() does not clear list of apps, so loadAppList
    // will append apps instead of replacing.
    states: [
      {
        id: '3-apps',
        trigger: (screen: any) => {
          screen.reset();
          screen.loadAppList([
            {
              title: 'gApp1',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
            },
            {
              title: 'anotherGapp2',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
              description: 'Short description',
            },
            {
              title: 'anotherGapp3',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
              // Current limitation is 80 characters.
              description:
                  'Lorem ipsum dolor sit amet, consectetur adipiscing elit,' +
                  ' sed do eiusmod tempor',
            },
          ]);
        },
      },
      {
        id: '5-apps',
        trigger: (screen: any) => {
          screen.reset();
          screen.loadAppList([
            {
              title: 'gApp1',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
            },
            {
              title: 'anotherGapp2',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
              description: 'Short description',
            },
            {
              title: 'anotherGapp3',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
              // Current limitation is 80 characters.
              description:
                  'Lorem ipsum dolor sit amet, consectetur adipiscing elit,' +
                  ' sed do eiusmod tempor',
            },
            {
              title: 'anotherGapp4',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
              // Current limitation is 80 characters.
              description:
                  'Lorem ipsum dolor sit amet, consectetur adipiscing elit,' +
                  ' sed do eiusmod tempor',
            },
            {
              title: 'anotherGapp5',
              icon_url: 'https://www.google.com/favicon.ico',
              category: 'Games',
              in_app_purchases: true,
              was_installed: false,
              content_rating: '',
              // Current limitation is 80 characters.
              description:
                  'Lorem ipsum dolor sit amet, consectetur adipiscing elit,' +
                  ' sed do eiusmod tempor',
            },
          ]);
        },
      },
    ],
  },
  {
    id: 'app-downloading',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'ai-intro',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'gemini-intro',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'parental-handoff',
    kind: ScreenKind.NORMAL,
    states: [{
      id: 'default',
      data: {
        username: 'TestUsername',
      },
    }],
  },
  {
    id: 'multidevice-setup-screen',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'gesture-navigation',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'kiosk-enable',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'choobe',
    kind: ScreenKind.NORMAL,
    handledSteps: 'overview',
    states: [
      {
        id: 'overview',
        data: {
          screens: [
            {
              screenID: 'screenID1',
              icon: 'oobe-40:scroll-choobe',
              title: 'choobeTouchpadScrollTitle',
              subtitle: 'choobeTouchpadScrollSubtitleEnabled',
              is_synced: true,
              is_revisitable: true,
              selected: false,
              is_completed: false,
            },
            {
              screenID: 'screenID2',
              icon: 'oobe-40:drive-pinning-choobe',
              title: 'choobeDrivePinningTitle',
              is_synced: false,
              is_revisitable: false,
              selected: false,
              is_completed: false,
            },
            {
              screenID: 'screenID3',
              icon: 'oobe-40:display-size-choobe',
              title: 'choobeDisplaySizeTitle',
              is_synced: false,
              is_revisitable: false,
              selected: false,
              is_completed: false,
            },
            {
              screenID: 'screenID4',
              icon: 'oobe-40:theme-choobe',
              title: 'choobeThemeSelectionTitle',
              is_synced: false,
              is_revisitable: false,
              selected: false,
              is_completed: false,
            },
          ],
        },
      },
    ],
  },
  {
    id: 'touchpad-scroll',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'drive-pinning',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'categories-selection',
    kind: ScreenKind.NORMAL,
    handledSteps: 'overview',
    states: [
      {
        id: 'overview',
        trigger: (screen: any) => {
          screen.setUIStep('overview');
          screen.setCategoriesData(createCategoriesData());
        },
      },
    ],
  },
  {
    id: 'personalized-apps',
    kind: ScreenKind.NORMAL,
    handledSteps: 'overview',
    states: [
      {
        id: 'overview',
        trigger: (screen: any) => {
          screen.setUIStep('overview');
          screen.setAppsAndUseCasesData(createCategoriesAppsData());
        },
      },
    ],
  },
  {
    id: 'marketing-opt-in',
    kind: ScreenKind.NORMAL,
    handledSteps: 'overview',
    states: [
      {
        id: 'WithOptionToSubscribe',
        data: {
          optInVisibility: true,
          optInDefaultState: true,
          legalFooterVisibility: false,
          cloudGamingDevice: false,
        },
        trigger: (screen: any) => {
          screen.setUIStep('overview');
          screen.updateA11ySettingsButtonVisibility(false);
        },
      },
      {
        id: 'NoOptionToSubscribe',
        data: {
          optInVisibility: false,
          optInDefaultState: false,
          legalFooterVisibility: false,
          cloudGamingDevice: false,
        },
        trigger: (screen: any) => {
          screen.setUIStep('overview');
          screen.updateA11ySettingsButtonVisibility(false);
        },
      },
      {
        id: 'WithLegalFooter',
        data: {
          optInVisibility: true,
          optInDefaultState: true,
          legalFooterVisibility: true,
          cloudGamingDevice: false,
        },
        trigger: (screen: any) => {
          screen.setUIStep('overview');
          screen.updateA11ySettingsButtonVisibility(false);
        },
      },
      {
        id: 'WithAccessibilityButton',
        data: {
          optInVisibility: true,
          optInDefaultState: true,
          legalFooterVisibility: true,
          cloudGamingDevice: false,
        },
        trigger: (screen: any) => {
          screen.setUIStep('overview');
          screen.updateA11ySettingsButtonVisibility(true);
        },
      },
    ],
  },
  {
    id: 'display-size',
    kind: ScreenKind.NORMAL,
    handledSteps: 'overview',
    states: [
      {
        id: 'overview',
        data: {
          availableSizes: [
            0.8999999761581421,
            1,
            1.0499999523162842,
            1.100000023841858,
            1.149999976158142,
            1.2000000476837158,
            1.25,
            1.2999999523162842,
            1.5,
          ],
          currentSize: 1,
        },
      },
    ],
  },
  {
    id: 'remote-activity-notification',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'cryptohome-recovery',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'quick-start',
    kind: ScreenKind.NORMAL,
    handledSteps: 'verification,connecting_to_wifi,signing_in,setup_complete',
    states: [
      {
        id: 'PinVerification',
        trigger: (screen: any) => {
          screen.setDiscoverableName('Chromebook (123)');
          screen.setPin('1234');
        },
      },
      {
        id: 'QRVerification',
        trigger: (screen: any) => {
          screen.setQRCode(createQuickStartQR());
        },
      },
      {
        id: 'ConnectingToWifi',
        trigger: (screen: any) => {
          screen.showConnectingToWifi();
        },
      },
      {
        id: 'SigninInEmailOnly',
        trigger: (screen: any) => {
          screen.showSigningInStep();
          screen.setUserEmail('test_email@gmail.com');
        },
      },
      {
        id: 'SigninInFullAvatar',
        trigger: (screen: any) => {
          screen.showSigningInStep();
          screen.setUserEmail('test_email@gmail.com');
          screen.setUserFullName('Test User');
          screen.setUserAvatarUrl(
              'https://lh3.googleusercontent.com/a/ACg8ocISjvU-p0Gz_kIBamP3jit_Y8PrQVU4AbIvQrUEZ04d=s96-c');
        },
      },
      {
        id: 'SetupComplete',
        trigger: (screen: any) => {
          screen.setUserEmail('test_email@gmail.com');
          screen.showSetupCompleteStep();
        },
      },
    ],
  },
  {
    id: 'user-allowlist-check-screen',
    kind: ScreenKind.NORMAL,
  },
  {
    id: 'online-authentication-screen',
    kind: ScreenKind.NORMAL,
  },
];

class DebugButton {
  constructor(parent: HTMLElement, title: string, callback: () => void) {
    this.element = (document.createElement('div')) as HTMLDivElement;
    this.element.textContent = title;

    this.element.className = 'debug-tool-button';
    this.element.addEventListener('click', this.onClick.bind(this));

    parent.appendChild(this.element);

    this.callback_ = callback;
    // In most cases we want to hide debugger UI right after making some
    // change to OOBE UI. However, more complex scenarios might want to
    // override this behavior.
    this.postCallback_ = () => DebuggerUi.getInstance().hideDebugUi();
  }

  element: HTMLDivElement;
  private callback_: (() => void);
  private postCallback_: (() => void);

  private onClick(): void {
    if (this.callback_) {
      this.callback_();
    }
    if (this.postCallback_) {
      this.postCallback_();
    }
  }
}

class ToolPanel {
  constructor(parent: HTMLElement|undefined, title: string, id: string) {
    this.titleDiv = (document.createElement('h2')) as HTMLHeadingElement;
    this.titleDiv.textContent = title;

    const panel = (document.createElement('div')) as HTMLDivElement;
    panel.className = 'debug-tool-panel';
    panel.id = id;
    panel.setAttribute('aria-hidden', 'true');

    assertInstanceof(parent, HTMLElement);
    parent.appendChild(this.titleDiv);
    parent.appendChild(panel);
    this.content = panel;
  }

  private titleDiv: HTMLHeadingElement;
  content: HTMLDivElement;

  show(): void {
    this.titleDiv.removeAttribute('hidden');
    this.content.removeAttribute('hidden');
  }

  clearContent(): void {
    const range = document.createRange();
    range.selectNodeContents(this.content);
    range.deleteContents();
  }

  hide(): void {
    this.titleDiv.setAttribute('hidden', 'true');
    this.content.setAttribute('hidden', 'true');
  }
}

export class DebuggerUi {
  constructor() {
    this.debuggerVisible_ = false;
    /** Element with Debugger UI */
    this.debuggerOverlay_ = undefined;
    this.debuggerButton_ = undefined;
    this.screenButtons = {};
    /**
     * ID of screen OOBE is currently in. Note that it is only updated upon
     * opening overlay and during screenshot series.
     */
    this.currentScreenId_ = undefined;
    /** ID of last screen on which Debugger UI was displayed */
    this.stateCachedFor_ = undefined;
    /** ID of assumed current screen state */
    this.lastScreenState_ = undefined;
    /** screen ID to screen definition mapping */
    this.screenMap = {};
    /** Ordered screen definitions, created on first show */
    this.knownScreens = undefined;
    /** Iterator for making a series of screenshots */
    this.commandIterator_ = undefined;
  }

  private debuggerVisible_: boolean;
  private debuggerOverlay_: HTMLDivElement|undefined;
  private debuggerButton_: HTMLDivElement|undefined;
  private screensPanel: ToolPanel|undefined;
  private screenButtons: Record<string, DebugButton>;
  private statesPanel: ToolPanel|undefined;
  private currentScreenId_: string|undefined;
  private stateCachedFor_: string|undefined;
  private lastScreenState_: string|undefined;
  private screenMap: Record<string, ScreenDefType>;
  private knownScreens: ScreenDefType[]|undefined;
  private commandIterator_: Generator<[() => void, number]>|undefined;

  get currentScreenId() {
    return this.currentScreenId_;
  }

  showDebugUi(): void {
    if (this.debuggerVisible_) {
      return;
    }
    this.refreshScreensPanel();
    this.debuggerVisible_ = true;
    assert(this.debuggerOverlay_);
    this.debuggerOverlay_.removeAttribute('hidden');
  }

  hideDebugUi(): void {
    this.debuggerVisible_ = false;
    assert(this.debuggerOverlay_);
    this.debuggerOverlay_.setAttribute('hidden', 'true');
  }

  toggleDebugUi(): void {
    if (this.debuggerVisible_) {
      this.hideDebugUi();
    } else {
      this.showDebugUi();
    }
  }

  private getScreenshotId() {
    let result = 'unknown';
    if (this.currentScreenId_) {
      result = this.currentScreenId_;
    }
    if (this.lastScreenState_ && this.lastScreenState_ !== 'default') {
      result = result + '_' + this.lastScreenState_;
    }
    return result;
  }

  /**
   * Function that, given a sequence of (function, delay) pairs, invokes
   * function with a delay before executing next one.
   */
  private runIterator_() {
    if (!this.commandIterator_) {
      return;
    }
    const command = this.commandIterator_.next();
    if (command.done) {
      this.commandIterator_ = undefined;
      return;
    }
    const [func, timeout] = command.value;
    try {
      func();
    } finally {
      setTimeout(() => {
        this.runIterator_();
      }, timeout);
    }
  }

  private hideButtonCommand(): [() => void, number] {
    return [
      () => {
        assertInstanceof(this.debuggerButton_, HTMLElement);
        this.debuggerButton_.setAttribute('hidden', 'true');
      },
      BUTTON_COMMAND_DELAY,
    ];
  }

  private showButtonCommand(): [() => void, number] {
    return [
      () => {
        assertInstanceof(this.debuggerButton_, HTMLElement);
        this.debuggerButton_.removeAttribute('hidden');
      },
      BUTTON_COMMAND_DELAY,
    ];
  }

  private showStateCommand(screenAndState: [string, string]):
      [() => void, number] {
    const [screenId, stateId] = screenAndState;
    // Switch to screen.
    return [
      () => {
        this.triggerScreenState(screenId, stateId);
      },
      SCREEN_LOADING_DELAY,
    ];
  }

  private makeScreenshotCommand(): [() => void, number] {
    // Make a screenshot.
    const id = this.getScreenshotId();
    return [
      () => {
        console.info('Making screenshot for ' + id);
        chrome.send('debug.captureScreenshot', [id]);
      },
      SCREENSHOT_CAPTURE_DELAY,
    ];
  }

  /**
   * Generator that returns commands to take screenshots for each
   * (screen, state) pair provided.
   */
  * screenshotSeries(statesList: Generator<[string, string]>):
          Generator<[() => void, number]> {
    yield this.hideButtonCommand();
    // Make all screenshots
    for (const screenAndState of statesList) {
      yield this.showStateCommand(screenAndState);
      yield this.makeScreenshotCommand();
    }
    yield this.showButtonCommand();
  }

  /**
   * Generator that returns commands to take screenshot of current state
   */
  * screenshotCurrent(): Generator<[() => void, number]> {
    yield this.hideButtonCommand();
    yield this.makeScreenshotCommand();
    yield this.showButtonCommand();
  }

  /**
   * Generator that returns all (screen, state) pairs for current screen.
   */
  * iterateStates(screenId: string): Generator<[string, string]> {
    assert(screenId, 'Screen must be defined when taking screenshots');
    for (const state of this.screenMap[screenId].states || []) {
      yield [screenId, state.id];
    }
  }

  /**
   * Generator that returns (screen, state) pairs for all known screens.
   */
  * iterateScreens(): Generator<[string, string]> {
    for (const screen of this.knownScreens || []) {
      if (screen.skipScreenshots) {
        continue;
      }
      yield* this.iterateStates(screen.id);
    }
  }

  private makeScreenshot(): void {
    this.hideDebugUi();
    this.commandIterator_ = this.screenshotCurrent();
    this.runIterator_();
  }

  private makeScreenshotsForCurrentScreen(): void {
    this.hideDebugUi();
    assert(this.currentScreenId_);
    this.commandIterator_ =
        this.screenshotSeries(this.iterateStates(this.currentScreenId_));
    this.runIterator_();
  }

  private makeScreenshotDeck(): void {
    this.hideDebugUi();
    this.commandIterator_ = this.screenshotSeries(this.iterateScreens());
    this.runIterator_();
  }

  private preProcessScreens(): void {
    KNOWN_SCREENS.forEach((screen, index) => {
      // Screen ordering
      screen!.index = index;
      // Create a default state
      if (!('states' in screen)) {
        screen!.states = [{
          id: 'default',
        }];
      }
      // Assign "default" state for each screen
      if (!screen.defaultState && screen.states) {
        screen.defaultState = screen.states[0].id;
      }
      screen!.stateMap = {};
      // For each state fall back to screen data if state data is not defined.
      for (const state of screen.states || []) {
        if (!('data' in state)) {
          state!.data = screen.data;
        }
        screen.stateMap[state.id] = state;
      }
    });
  }

  private toggleGameMode(): void {
    KNOWN_SCREENS.forEach((screen, _index) => {
      if (screen.id === 'marketing-opt-in' && screen.states) {
        for (const state of screen.states) {
          if (state.data) {
            state.data.cloudGamingDevice = !(state.data.cloudGamingDevice);
          }
        }
      }
    });

    this.triggerScreenState(this.currentScreenId_, this.lastScreenState_);
  }

  private createLanguagePanel(): void {
    const langPanel = new ToolPanel(
        this.debuggerOverlay_, 'Language', 'DebuggerPanelLanguage');
    const LANGUAGES = [
      ['English', 'en-US'],
      ['German', 'de'],
      ['Russian', 'ru'],
      ['Herbew (RTL)', 'he'],
      ['Arabic (RTL)', 'ar'],
      ['Chinese', 'zh-TW'],
      ['Japanese', 'ja'],
    ];
    LANGUAGES.forEach(function(pair) {
      new DebugButton(langPanel.content, pair[0], function(locale: string) {
        chrome.send('login.WelcomeScreen.userActed', ['setLocaleId', locale]);
      }.bind(null, pair[1]));
    });
  }

  private createToolsPanel(): void {
    const panel =
        new ToolPanel(this.debuggerOverlay_, 'Tools', 'DebuggerPanelTools');
    new DebugButton(
        panel.content, 'Capture screenshot', this.makeScreenshot.bind(this));
    new DebugButton(
        panel.content, 'Capture all states of screen',
        this.makeScreenshotsForCurrentScreen.bind(this));
    new DebugButton(
        panel.content, 'Capture deck of all screens',
        this.makeScreenshotDeck.bind(this));
    new DebugButton(panel.content, 'Toggle color mode', () => {
      chrome.send('debug.toggleColorMode');
    });
    const button = new DebugButton(
        panel.content, 'Toggle gaming mode', this.toggleGameMode.bind(this));

    button.element.classList.add('gametoggle-button');
  }

  private createWallpaperPanel(): void {
    const wallpaperPanel = new ToolPanel(
        this.debuggerOverlay_, 'Wallpaper', 'DebuggerPanelWallpaper');
    const WALLPAPERS = [
      ['Default', 'def'],
      ['White', 'wh'],
      ['Black', 'bk'],
      ['Red', 'r'],
      ['Blue', 'bl'],
      ['Green', 'gn'],
      ['Yellow', 'ye'],
    ];
    WALLPAPERS.forEach(function(pair) {
      new DebugButton(wallpaperPanel.content, pair[0], function(color: string) {
        chrome.send('debug.switchWallpaper', [color]);
      }.bind(null, pair[1]));
    });
  }

  private createScreensPanel(): void {
    const panel =
        new ToolPanel(this.debuggerOverlay_, 'Screens', 'DebuggerPanelScreens');
    // List of screens will be created later, as not all screens
    // might be registered at this point.
    this.screensPanel = panel;
  }

  private createStatesPanel(): void {
    const panel = new ToolPanel(
        this.debuggerOverlay_, 'Screen States', 'DebuggerPanelStates');
    // List of states is rebuilt every time to reflect current screen.
    this.statesPanel = panel;
  }

  private switchToScreen(screen: ScreenDefType): void {
    this.triggerScreenState(screen.id, screen.defaultState);
  }

  private triggerScreenState(
      screenId: string|undefined, stateId: string|undefined): void {
    assert(screenId, 'Screen must be defined when trying to switch to it');
    const screen = this.screenMap[screenId];
    assert(stateId, 'Screen state must be defined when trying to switch to it');
    assert(screen.stateMap,
        'Screen must contain steps when trying to switch to it');
    const state = screen.stateMap[stateId];

    // Disable userActed from triggering chrome.send() and crashing.
    const screenElem = document.getElementById(screenId);
    if (screenElem && 'userActed' in screenElem &&
        typeof screenElem.userActed === 'function') {
      screenElem.userActed = () => {};
    }

    this.currentScreenId_ = screenId;
    this.lastScreenState_ = stateId;
    const displayManager = Oobe.getInstance();
    displayManager.showScreen({id: screen.id, data: state.data || {}});
    if (state.trigger) {
      state.trigger(displayManager.currentScreen);
    }
  }

  private createScreensList(): void {
    for (const screen of KNOWN_SCREENS) {
      this.screenMap[screen.id] = screen as ScreenDefType;
    }
    this.knownScreens = [];
    this.screenButtons = {};
    for (const id of Oobe.getInstance().screens) {
      if (id in this.screenMap) {
        const screenDef = this.screenMap[id];
        const screenElement = $(id);
        if ('listSteps' in screenElement &&
            typeof screenElement.listSteps === 'function') {
          if (screenDef.stateMap && 'default' in screenDef.stateMap) {
            screenDef.states = [];
            screenDef.stateMap = {};
          }
          const handledSteps = new Set();
          if (screenDef.handledSteps) {
            for (const step of screenDef.handledSteps.split(',')) {
              handledSteps.add(step);
            }
          }
          for (const step of screenElement.listSteps()) {
            if (handledSteps.has(step)) {
              continue;
            }
            const state = {
              id: 'step-' + step,
              data: screenDef.data,
              trigger: (screen: any) => {
                screen.setUIStep(step);
              },
            };
            screenDef!.states!.push(state);
            screenDef!.stateMap![state.id] = state;
          }
          if (screenDef.defaultState === 'default' &&
              'defaultUIStep' in screenElement &&
              typeof screenElement.defaultUIStep === 'function') {
            screenDef.defaultState = 'step-' + screenElement.defaultUIStep();
          }
        }
        this.knownScreens.push(screenDef);
        this.screenMap[id] = screenDef;
      } else {
        console.error('### Screen not registered in debug overlay ' + id);
        const unknownScreen = {
          id: id,
          kind: ScreenKind.UNKNOWN,
          suffix: '???',
          index: this.knownScreens.length + 1000,
          defaultState: 'unknown',
          states: [{
            id: 'unknown',
          }],
        };
        this.knownScreens.push(unknownScreen);
        this.screenMap[id] = unknownScreen;
      }
    }
    this.knownScreens =
        this.knownScreens.sort((a: ScreenDefType, b: ScreenDefType) => {
          return (a.index || 0) - (b.index || 0);
        });
    assertInstanceof(this.screensPanel, ToolPanel);
    const content = this.screensPanel.content;
    this.knownScreens.forEach((screen) => {
      let name = screen.id;
      if (screen.suffix) {
        name = name + ' (' + screen.suffix + ')';
      }
      const button = new DebugButton(
          content, name, this.switchToScreen.bind(this, screen));
      button.element.classList.add('debug-button-' + screen.kind);
      this.screenButtons[screen.id] = button;
    });
  }

  private refreshScreensPanel(): void {
    if (this.knownScreens === undefined) {
      this.createScreensList();
    }
    const displayManager = Oobe.getInstance();
    if (this.stateCachedFor_) {
      this.screenButtons[this.stateCachedFor_].element.classList.remove(
          'debug-button-selected');
    }
    if (displayManager.currentScreen) {
      if (this.currentScreenId_ !== displayManager.currentScreen.id) {
        this.currentScreenId_ = displayManager.currentScreen.id;
        this.lastScreenState_ = undefined;
      }

      this.stateCachedFor_ = displayManager.currentScreen.id;
      this.screenButtons[this.stateCachedFor_].element.classList.add(
          'debug-button-selected');
    }

    assert(this.currentScreenId_);
    const screen = this.screenMap[this.currentScreenId_];

    assert(this.statesPanel);
    this.statesPanel.clearContent();
    for (const state of screen.states || []) {
      const button = new DebugButton(
          this.statesPanel.content, state.id,
          this.triggerScreenState.bind(this, this.currentScreenId_, state.id));
      if (state.id === this.lastScreenState_) {
        button.element.classList.add('debug-button-selected');
      }
    }

    if (this.currentScreenId_ === 'marketing-opt-in') {
      document.getElementsByClassName('gametoggle-button')[0].removeAttribute(
          'hidden');
    } else {
      document.getElementsByClassName('gametoggle-button')[0].setAttribute(
          'hidden', 'true');
    }

    this.statesPanel.show();
  }

  private createCssStyle(name: string, styleSpec: string): void {
    const style = document.createElement('style') as HTMLStyleElement;
    style.innerHTML = sanitizeInnerHtml('.' + name + ' {' + styleSpec + '}');
    document.getElementsByTagName('head')[0].appendChild(style);
  }

  register(element: HTMLElement): void {
    // Pre-process Screens data
    this.preProcessScreens();
    // Create CSS styles
    {
      this.createCssStyle('debugger-button', DEBUG_BUTTON_STYLE);
      this.createCssStyle('debugger-overlay', DEBUG_OVERLAY_STYLE);
      this.createCssStyle('debug-tool-panel', TOOL_PANEL_STYLE);
      this.createCssStyle('debug-tool-button', TOOL_BUTTON_STYLE);
      this.createCssStyle('debug-button-selected', TOOL_BUTTON_SELECTED_STYLE);
      this.createCssStyle('debug-button-normal', SCREEN_BUTTON_STYLE_NORMAL);
      this.createCssStyle('debug-button-error', SCREEN_BUTTON_STYLE_ERROR);
      this.createCssStyle('debug-button-other', SCREEN_BUTTON_STYLE_OTHER);
      this.createCssStyle('debug-button-unknown', SCREEN_BUTTON_STYLE_UNKNOWN);
    }
    {
      // Create UI Debugger button
      const button = document.createElement('div') as HTMLDivElement;
      button.id = 'invokeDebuggerButton';
      button.className = 'debugger-button';
      button.textContent = 'Debug';
      button.addEventListener('click', this.toggleDebugUi.bind(this));

      this.debuggerButton_ = button;
    }
    {
      // Create base debugger panel.
      const overlay = (document.createElement('div')) as HTMLDivElement;
      overlay.id = 'debuggerOverlay';
      overlay.className = 'debugger-overlay';
      overlay.setAttribute('aria-label', 'OOBE debug overlay');
      overlay.setAttribute('hidden', 'true');
      this.debuggerOverlay_ = overlay;
    }
    this.createLanguagePanel();
    this.createScreensPanel();
    this.createStatesPanel();
    this.createToolsPanel();
    this.createWallpaperPanel();

    element.appendChild(this.debuggerButton_);
    element.appendChild(this.debuggerOverlay_);
  }

  static getInstance(): DebuggerUi {
    return instance || (instance = new DebuggerUi());
  }
}

let instance: DebuggerUi|undefined = undefined;
