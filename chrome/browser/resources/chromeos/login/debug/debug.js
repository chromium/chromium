// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Root element of the OOBE UI Debugger.
 */

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
// #import {loadTimeData} from '../i18n_setup.js';
// #import {Oobe} from '../cr_ui.m.js'
// #import {$} from 'chrome://resources/js/util.m.js';
// #import './debug_util.js';

// #import {MessageType, ProblemType} from 'chrome://resources/cr_components/chromeos/quick_unlock/setup_pin_keyboard.m.js';

cr.define('cr.ui.login.debug', function() {
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

  /**
   * Indicates if screen is present in usual user flow, represents some error
   * state or is shown in some other cases. See KNOWN_SCREENS for more details.
   * @enum {string}
   */
  const ScreenKind = {
    NORMAL: 'normal',
    ERROR: 'error',
    OTHER: 'other',
    UNKNOWN: 'unknown',
  };

  const BUTTON_COMMAND_DELAY = 100;
  const SCREEN_LOADING_DELAY = 500;
  const SCREENSHOT_CAPTURE_DELAY = 200;

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
  const KNOWN_SCREENS = [
    {
      // Device without mouse/keyboard.
      id: 'hid-detection',
      kind: ScreenKind.NORMAL,
      states: [
        {
          id: 'searching',
          trigger: (screen) => {
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
          trigger: (screen) => {
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
          trigger: (screen) => {
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
          trigger: (screen) => {
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
          trigger: (screen) => {
            screen.closeTimezoneSection_();
          },
        },
        {
          id: 'acessibility',
          trigger: (screen) => {
            screen.onWelcomeAccessibilityButtonClicked_();
          },
        },
        {
          id: 'languages',
          trigger: (screen) => {
            screen.onWelcomeSelectLanguageButtonClicked_();
          },
        },
        {
          // Available on Chromebox for meetings.
          id: 'timezone',
          trigger: (screen) => {
            screen.onWelcomeTimezoneButtonClicked_();
          },
        },
        {
          // Advanced options, shown by long press on "Welcome!" title.
          id: 'adv-options',
          trigger: (screen) => {
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
          trigger: (screen) => {
            screen.updateCountdownString(
                'Your device will shut down in 60 seconds. Remove the USB \
                 before turning your device back on. Then you can start using \
                 ChromeOS Flex.');
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
          trigger: (screen) => {
            screen.setError(
                'ChromeOS was unable to connect to Public Wifi. ' +
                'Please select another network or try again.');
          }
        },
      ],
    },
    {
      id: 'oobe-eula-md',
      kind: ScreenKind.NORMAL,
      // TODO: Current logic triggers switching screens in focus(), making
      // it impossible to trigger  installation settings dialog.
      skipScreenshots: true,
    },
    {
      id: 'demo-setup',
      kind: ScreenKind.OTHER,
      suffix: 'demo',
      handledSteps: 'progress,error',
      states: [
        {
          id: 'download-resources',
          trigger: (screen) => {
            screen.setCurrentSetupStep('downloadResources');
          },
        },
        {
          id: 'enrollment',
          trigger: (screen) => {
            screen.setCurrentSetupStep('enrollment');
          },
        },
        {
          id: 'done',
          trigger: (screen) => {
            screen.setCurrentSetupStep('complete');
            screen.onSetupSucceeded();
          },
        },
        {
          id: 'error',
          trigger: (screen) => {
            screen.onSetupFailed('Some error message', true);
          },
        },
      ],
    },
    {
      id: 'oobe-update',
      kind: ScreenKind.NORMAL,
      states: [
        {
          // Checking for update
          id: 'check-update',
        },
        {
          // Ask for permission to update over celluar
          id: 'require-permission-celluar',
          trigger: (screen) => {
            screen.onBeforeShow();
            screen.setUpdateState('cellular');
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
          trigger: (screen) => {
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
          trigger: (screen) => {
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
      id: 'offline-ad-login',
      kind: ScreenKind.NORMAL,
      // Remove this step from preview here, because it can only occur during
      // enterprise enrollment step and it is already available there in debug
      // overlay.
      handledSteps: 'unlock,creds',
      suffix: 'E',
      states: [
        {
          id: 'unlock',
          trigger: (screen) => {
            screen.setUIStep('unlock');
          },
          data: {},
        },
        {
          id: 'creds',
          trigger: (screen) => {
            screen.setUIStep('creds');
          },
          data: {},
        },
      ],
    },
    {
      id: 'enterprise-enrollment',
      kind: ScreenKind.NORMAL,
      defaultState: 'step-signin',
      handledSteps: 'error,ad-join',
      suffix: 'E',
      states: [
        {
          id: 'error',
          trigger: (screen) => {
            screen.showError('Some error message', true);
          },
        },
        {
          id: 'ad-join-encrypted',
          trigger: (screen) => {
            screen.setAdJoinParams('machineName', 'userName', 0, true);
            screen.showStep('ad-join');
          },
        },
        {
          id: 'ad-join',
          trigger: (screen) => {
            screen.setAdJoinParams('machineName', 'userName', 0, false);
            screen.showStep('ad-join');
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
          trigger: (screen) => {
            screen.setUIState(2);     // signin
            screen.setErrorState(2);  // offline
            screen.allowGuestSignin(true);
            screen.allowOfflineLogin(true);
          }
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
          trigger: (screen) => {
            screen.setUIState(0);
            screen.setEnterpriseAndDeviceName('example.com', 'Chromebook');
          },
        },
        {
          id: 'checking-for-update',
          trigger: (screen) => {
            screen.setUIState(1);
            screen.setUpdateProgressUnavailable(true);
            screen.setEstimatedTimeLeftVisible(false);
            screen.setUpdateProgressValue(0);
            screen.setUpdateProgressMessage('Checking for update');
          },
        },
        {
          id: 'in-progress',
          trigger: (screen) => {
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
          trigger: (screen) => {
            screen.setUIState(5);
            screen.setEolMessage(
                'Message from admin: please return device somewhere.');
          },
        },
      ],
    },
    {
      id: 'autolaunch',
      kind: ScreenKind.OTHER,
      suffix: 'kiosk',
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
      ]
    },
    {
      id: 'smart-privacy-protection',
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
          trigger: (screen) => {
            screen.reset();
            screen.setScreenState(0);
          },
        },
        {
          id: 'revert-promise',
          trigger: (screen) => {
            screen.reset();
            screen.setScreenState(1);
          },
        },
        {
          id: 'powerwash-proposal',
          trigger: (screen) => {
            screen.reset();
            screen.setScreenState(2);
          },
        },
        {
          id: 'powerwash-rollback',
          trigger: (screen) => {
            screen.reset();
            screen.setScreenState(2);
            screen.setIsRollbackAvailable(true);
            screen.setIsRollbackRequested(true);
            screen.setIsTpmFirmwareUpdateAvailable(true);
          },
        },
        {
          id: 'powerwash-confirmation',
          trigger: (screen) => {
            screen.reset();
            screen.setShouldShowConfirmationDialog(true);
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
      handledSteps: 'allowlist-error',
      states: [
        {
          id: 'allowlist-customer',
          trigger: (screen) => {
            screen.showAllowlistCheckFailedError({
              enterpriseManaged: false,
            });
          },
        },
      ],
    },
    {
      id: 'offline-login',
      kind: ScreenKind.NORMAL,
      states: [
        {
          id: 'default',
          trigger: (screen) => {
            screen.loadParams({});
          },
        },
        {
          // kAccountsPrefLoginScreenDomainAutoComplete value is set
          id: 'offline-gaia-domain',
          trigger: (screen) => {
            screen.loadParams({
              emailDomain: 'somedomain.com',
            });
          },
        },
        {
          // Device is enterprise-managed.
          id: 'offline-gaia-enterprise',
          trigger: (screen) => {
            screen.loadParams({
              enterpriseDomainManager: 'example.com',
            });
          },
        },
        {
          // Password and email mismatch error message.
          id: 'offline-login-password-mismatch',
          trigger: (screen) => {
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
          trigger: (screen) => {
            screen.show(
                'Sign-in failed because your password could not be ' +
                    'verified. Please contact your administrator or try again.',
                'Try again');
          },
        },
      ],
    },
    {
      // GAIA password changed.
      id: 'gaia-password-changed',
      kind: ScreenKind.OTHER,
      handledSteps: 'password',
      data: {
        email: 'someone@example.com',
      },
      states: [
        {
          // No error
          id: 'no-error',
          trigger: (screen) => {
            screen.onBeforeShow({
              email: 'someone@example.com',
            });
          },
        },
        {
          // Has error
          id: 'has-error',
          trigger: (screen) => {
            screen.onBeforeShow({
              email: 'someone@example.com',
              showError: true,
            });
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
          trigger: (screen) => {
            const error = 'Some error text';
            screen.showErrorDialog(error);
          }
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
          trigger: (screen) => {
            screen.setUIState(1);
            screen.setBatteryState(100, true, true);
            screen.setNecessaryBatteryPercent(40);
          },
        },
        {
          id: 'ready-low-battery',
          trigger: (screen) => {
            screen.setUIState(1);
            screen.setBatteryState(11, false, false);
            screen.setNecessaryBatteryPercent(40);
          },
        },
        {
          id: 'migrating',
          trigger: (screen) => {
            screen.setUIState(2);
            screen.setMigrationProgress(0.37);
          },
        },
        {
          id: 'not-enough-space',
          trigger: (screen) => {
            screen.setUIState(4);
            screen.setSpaceInfoInString(
                '1 GB' /* availableSpaceSize */,
                '2 GB' /* necessarySpaceSize */);
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
          trigger: (screen) => {
            screen.show(
                'someone@example.com',
                false,  // manualPasswordInput
                0,      // attempt count
                () => {});
          },
        },
        {
          // Password was scraped
          id: 'scraped-retry',
          trigger: (screen) => {
            screen.show(
                'someone@example.com',
                false,  // manualPasswordInput
                1,      // attempt count
                () => {});
          },
        },
        {
          // No password was scraped
          id: 'manual',
          trigger: (screen) => {
            screen.show(
                'someone@example.com',
                true,  // manualPasswordInput
                0,     // attempt count
                () => {});
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
          trigger: (screen) => {
            screen.setArcTransition(2);
            screen.setUIStep('progress');
          },
        },
        {
          id: 'remove-supervision',
          trigger: (screen) => {
            screen.setArcTransition(1);
            screen.setUIStep('progress');
          },
        },
        {
          id: 'add-management',
          trigger: (screen) => {
            screen.setArcTransition(3);
            screen.setManagementEntity('example.com');
            screen.setUIStep('progress');
          }
        },
        {
          id: 'add-management-unknown-admin',
          trigger: (screen) => {
            screen.setArcTransition(3);
            screen.setManagementEntity('');
            screen.setUIStep('progress');
          }
        },
        {
          id: 'error-supervision',
          trigger: (screen) => {
            screen.setArcTransition(1);
            screen.setUIStep('error');
          }
        },
        {
          id: 'error-management',
          trigger: (screen) => {
            screen.setArcTransition(3);
            screen.setUIStep('error');
          }
        },
      ],
    },
    {
      id: 'lacros-data-migration',
      kind: ScreenKind.OTHER,
      defaultState: 'default',
      handledSteps: 'skip-revealed',
      states: [{
        id: 'skip-revealed',
        trigger: (screen) => {
          screen.showSkipButton();
        }
      }],
    },
    {
      id: 'terms-of-service',
      kind: ScreenKind.NORMAL,
      handledSteps: 'loading,loaded,error',
      states: [
        {
          id: 'loading',
          trigger: (screen) => {
            screen.onBeforeShow({manager: 'TestCompany'});
            screen.setUIStep('loading');
          },
        },
        {
          id: 'loaded',
          trigger: (screen) => {
            screen.onBeforeShow({manager: 'TestCompany'});
            screen.setTermsOfService('TOS BEGIN\nThese are the terms\nTOS END');
          },
        },
        {
          id: 'error',
          trigger: (screen) => {
            screen.setTermsOfServiceLoadError();
          },
        },
      ],
    },
    {
      id: 'sync-consent',
      kind: ScreenKind.NORMAL,
      defaultState: 'step-loaded',
      states: [
        {
          id: 'minor-mode',
          data: {
            isChildAccount: true,
            isArcRestricted: false,
          },
          trigger: (screen) => {
            screen.setIsMinorMode(true);
          },
        },
        {
          id: 'arc-restricted',
          data: {
            isChildAccount: false,
            isArcRestricted: true,
          },
        }
      ]
    },
    {
      id: 'consolidated-consent',
      kind: ScreenKind.NORMAL,
      handledSteps: 'loaded,loading,error,google-eula,cros-eula,arc,privacy',
      // TODO(crbug.com/1247174): Use localized URLs for eulaUrl and
      // additionalTosUrl.
      states: [
        {
          id: 'regular-owner',
          trigger: (screen) => {
            screen.setIsDeviceOwner(true);
          },
          data: {
            isArcEnabled: true,
            isDemo: false,
            isChildAccount: false,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'regular',
          trigger: (screen) => {
            screen.setIsDeviceOwner(false);
          },
          data: {
            isArcEnabled: true,
            isDemo: false,
            isChildAccount: false,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'child-owner',
          trigger: (screen) => {
            screen.setIsDeviceOwner(true);
          },
          data: {
            isArcEnabled: true,
            isDemo: false,
            isChildAccount: true,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'child',
          trigger: (screen) => {
            screen.setIsDeviceOwner(false);
          },
          data: {
            isArcEnabled: true,
            isDemo: false,
            isChildAccount: true,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'demo-mode',
          data: {
            isArcEnabled: true,
            isDemo: true,
            isChildAccount: false,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'arc-disabled-owner',
          trigger: (screen) => {
            screen.setIsDeviceOwner(true);
          },
          data: {
            isArcEnabled: false,
            isDemo: false,
            isChildAccount: false,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'arc-disabled',
          trigger: (screen) => {
            screen.setIsDeviceOwner(false);
          },
          data: {
            isArcEnabled: false,
            isDemo: false,
            isChildAccount: false,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'managed-account',
          trigger: (screen) => {
            screen.setBackupMode(true, true);
            screen.setLocationMode(false, true);
            screen.setIsDeviceOwner(false);
          },
          data: {
            isArcEnabled: true,
            isDemo: false,
            isChildAccount: false,
            isEnterpriseManagedAccount: true,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },
        {
          id: 'error',
          trigger: (screen) => {
            screen.setUIStep('error');
            screen.setIsDeviceOwner(true);
          },
          data: {
            isArcEnabled: true,
            isDemo: false,
            isChildAccount: false,
            isEnterpriseManagedAccount: false,
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
            countryCode: 'us',
          },
        },

      ]
    },
    {
      id: 'guest-tos',
      kind: ScreenKind.NORMAL,
      handledSteps: 'loading,loaded,google-eula,cros-eula',
      // TODO(crbug.com/1247174): Use localized URLs for googleEulaURL and
      // crosEulaURL.
      states: [
        {
          id: 'loaded',
          trigger: (screen) => {
            screen.setUIStep('loaded');
          },
          data: {
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          },
        },
        {
          id: 'loading',
          trigger: (screen) => {
            screen.setUIStep('loading');
          },
          data: {
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          },
        },
        {
          id: 'google-eula',
          trigger: (screen) => {
            screen.setUIStep('google-eula');
          },
          data: {
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          },
        },
        {
          id: 'cros-eula',
          trigger: (screen) => {
            screen.setUIStep('cros-eula');
          },
          data: {
            googleEulaUrl: 'https://policies.google.com/terms/embedded?hl=en',
            crosEulaUrl: 'https://www.google.com/intl/en/chrome/terms/',
          },
        },
      ]
    },
    {
      id: 'hw-data-collection',
      kind: ScreenKind.OTHER,
    },
    {
      id: 'fingerprint-setup',
      kind: ScreenKind.NORMAL,
      defaultState: 'default',
      handledSteps: 'progress',
      states: [
        {
          id: 'progress-0',
          trigger: (screen) => {
            screen.onEnrollScanDone(0 /* success */, false, 0);
            screen.onEnrollScanDone(0 /* success */, false, 0);
          },
        },
        {
          id: 'error-immobile',
          trigger: (screen) => {
            screen.onEnrollScanDone(0 /* success */, false, 0);
            screen.onEnrollScanDone(6, false, 30);
          },
        },
        {
          id: 'progress-60',
          trigger: (screen) => {
            screen.onEnrollScanDone(0 /* success */, false, 0);
            screen.onEnrollScanDone(0 /* success */, false, 60);
          },
        },
        {
          id: 'progress-100',
          trigger: (screen) => {
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
          trigger: (screen) => {
            (screen.$).pinKeyboard.hideProblem_();
          }
        },
        {
          id: 'error-warning',
          trigger: (screen) => {
            (screen.$).pinKeyboard.showProblem_(
                MessageType.TOO_WEAK, ProblemType.WARNING);
          }
        },
        {
          id: 'error-error',
          trigger: (screen) => {
            (screen.$).pinKeyboard.showProblem_(
                MessageType.TOO_LONG, ProblemType.ERROR);
          }
        }
      ]
    },
    {
      id: 'arc-tos',
      kind: ScreenKind.NORMAL,
      states: [
        {
          id: 'us-terms-loaded',
          trigger: (screen) => {
            screen.clearDemoMode();
            screen.reloadPlayStoreToS();
          },
        },
        {
          id: 'demo-mode',
          trigger: (screen) => {
            screen.setupForDemoMode();
            screen.reloadPlayStoreToS();
          },
        },
      ],
    },
    // TODO(crbug.com/1261902): Remove.
    {
      id: 'recommend-apps-old',
      kind: ScreenKind.NORMAL,
      handledSteps: 'list',
      // Known issue: reset() does not clear list of apps, so loadAppList
      // will append apps instead of replacing.
      states: [
        {
          id: '2-apps',
          trigger: (screen) => {
            screen.reset();
            screen.setWebview(RECOMMENDED_APPS_OLD_CONTENT);
            screen.loadAppList([
              {
                name: 'Test app 1',
                package_name: 'test1.app',
              },
              {
                name: 'Test app 2 with some really long name',
                package_name: 'test2.app',
              },
            ]);
          },
        },
        {
          id: '21-apps',
          trigger: (screen) => {
            // There can be up to 21 apps: see recommend_apps_fetcher_impl
            screen.reset();
            screen.setWebview(RECOMMENDED_APPS_OLD_CONTENT);
            const apps = [];
            for (let i = 1; i <= 21; i++) {
              apps.push({
                name: 'Test app ' + i,
                package_name: 'app.test' + i,
              });
            }
            screen.loadAppList(apps);
          },
        },
      ],
    },
    {
      id: 'recommend-apps',
      kind: ScreenKind.NORMAL,
      handledSteps: 'list',
      // Known issue: reset() does not clear list of apps, so loadAppList
      // will append apps instead of replacing.
      states: [
        {
          id: '2-apps',
          trigger: (screen) => {
            screen.reset();
            screen.setWebview(RECOMMENDED_APPS_CONTENT);
            screen.loadAppList([
              {
                name: 'Test app 1',
                package_name: 'test1.app',
              },
              {
                name: 'Test app 2 with some really long name',
                package_name: 'test2.app',
              },
            ]);
          },
        },
        {
          id: '21-apps',
          trigger: (screen) => {
            // There can be up to 21 apps: see recommend_apps_fetcher_impl
            screen.reset();
            screen.setWebview(RECOMMENDED_APPS_CONTENT);
            const apps = [];
            for (let i = 1; i <= 21; i++) {
              apps.push({
                name: 'Test app ' + i,
                package_name: 'app.test' + i,
              });
            }
            screen.loadAppList(apps);
          },
        },
      ],
    },
    {
      id: 'app-downloading',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'assistant-optin-flow',
      kind: ScreenKind.NORMAL,
      states: [
        {
          id: 'loading',
          trigger: (screen) => {
            (screen.$).card.showStep('loading');
          },
        },
        {
          id: 'related_info skip_activity_control=true',
          trigger: (screen) => {
            ((screen.$).card.$).relatedInfo.skipActivityControl_ = true;
            (screen.$).card.showStep('related-info');
          },
        },
        {
          id: 'related_info skip_activity_control=false',
          trigger: (screen) => {
            ((screen.$).card.$).relatedInfo.skipActivityControl_ = false;
            (screen.$).card.showStep('related-info');
          },
        },
        {
          id: 'voice_match_begin',
          trigger: (screen) => {
            (screen.$).card.showStep('voice-match');
          },
        },
        {
          id: 'voice_match_listen',
          trigger: (screen) => {
            (screen.$).card.showStep('voice-match');
            ((screen.$).card.$).voiceMatch.setUIStep('recording');
            ((screen.$).card.$).voiceMatch.listenForHotword();
          },
        },
        {
          id: 'voice_match_done',
          trigger: (screen) => {
            (screen.$).card.showStep('voice-match');
            ((screen.$).card.$).voiceMatch.setUIStep('recording');
            ((screen.$).card.$).voiceMatch.voiceMatchDone();
          },
        },
      ],
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
          },
          trigger: (screen) => {
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
          },
          trigger: (screen) => {
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
          },
          trigger: (screen) => {
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
          },
          trigger: (screen) => {
            screen.setUIStep('overview');
            screen.updateA11ySettingsButtonVisibility(true);
          },
        },
      ],
    },
  ];

  class DebugButton {
    constructor(parent, title, callback) {
      this.element =
          /** @type {!HTMLElement} */ (document.createElement('div'));
      this.element.textContent = title;

      this.element.className = 'debug-tool-button';
      this.element.addEventListener('click', this.onClick.bind(this));

      parent.appendChild(this.element);

      this.callback_ = callback;
      // In most cases we want to hide debugger UI right after making some
      // change to OOBE UI. However, more complex scenarios might want to
      // override this behavior.
      this.postCallback_ = function() {
        cr.ui.login.debug.DebuggerUI.getInstance().hideDebugUI();
      };
    }

    onClick() {
      if (this.callback_) {
        this.callback_();
      }
      if (this.postCallback_) {
        this.postCallback_();
      }
    }
  }

  class ToolPanel {
    constructor(parent, title, id) {
      this.titleDiv =
          /** @type {!HTMLElement} */ (document.createElement('h2'));
      this.titleDiv.textContent = title;

      const panel = /** @type {!HTMLElement} */ (document.createElement('div'));
      panel.className = 'debug-tool-panel';
      panel.id = id;

      parent.appendChild(this.titleDiv);
      parent.appendChild(panel);
      this.content = panel;
    }

    show() {
      this.titleDiv.removeAttribute('hidden');
      this.content.removeAttribute('hidden');
    }

    clearContent() {
      const range = document.createRange();
      range.selectNodeContents(this.content);
      range.deleteContents();
    }

    hide() {
      this.titleDiv.setAttribute('hidden', true);
      this.content.setAttribute('hidden', true);
    }
  }

  /* #export */ class DebuggerUI {
    constructor() {
      this.debuggerVisible_ = false;
      /** Element with Debugger UI */
      this.debuggerOverlay_ = undefined;
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

    showDebugUI() {
      if (this.debuggerVisible_) {
        return;
      }
      this.refreshScreensPanel();
      this.debuggerVisible_ = true;
      this.debuggerOverlay_.removeAttribute('hidden');
    }

    hideDebugUI() {
      this.debuggerVisible_ = false;
      this.debuggerOverlay_.setAttribute('hidden', true);
    }

    toggleDebugUI() {
      if (this.debuggerVisible_) {
        this.hideDebugUI();
      } else {
        this.showDebugUI();
      }
    }

    getScreenshotId() {
      var result = 'unknown';
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
    runIterator_() {
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

    hideButtonCommand() {
      return [
        () => {
          this.debuggerButton_.setAttribute('hidden', true);
        },
        BUTTON_COMMAND_DELAY
      ];
    }

    showButtonCommand() {
      return [
        () => {
          this.debuggerButton_.removeAttribute('hidden');
        },
        BUTTON_COMMAND_DELAY
      ];
    }

    showStateCommand(screenAndState) {
      const [screenId, stateId] = screenAndState;
      // Switch to screen.
      return [
        () => {
          this.triggerScreenState(screenId, stateId);
        },
        SCREEN_LOADING_DELAY
      ];
    }

    makeScreenshotCommand() {
      // Make a screenshot.
      const id = this.getScreenshotId();
      return [
        () => {
          console.info('Making screenshot for ' + id);
          chrome.send('debug.captureScreenshot', [id]);
        },
        SCREENSHOT_CAPTURE_DELAY
      ];
    }

    /**
     * Generator that returns commands to take screenshots for each
     * (screen, state) pair provided.
     */
    * screenshotSeries_(statesList) {
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
    * screenshotCurrent() {
      yield this.hideButtonCommand();
      yield this.makeScreenshotCommand();
      yield this.showButtonCommand();
    }

    /**
     * Generator that returns all (screen, state) pairs for current screen.
     */
    * iterateStates(screenId) {
      for (const state of this.screenMap[screenId].states) {
        yield [screenId, state.id];
      }
    }

    /**
     * Generator that returns (screen, state) pairs for all known screens.
     */
    * iterateScreens() {
      for (const screen of this.knownScreens) {
        if (screen.skipScreenshots) {
          continue;
        }
        yield* this.iterateStates(screen.id);
      }
    }

    makeScreenshot() {
      this.hideDebugUI();
      this.commandIterator_ = this.screenshotCurrent();
      this.runIterator_();
    }

    makeScreenshotsForCurrentScreen() {
      this.hideDebugUI();
      this.commandIterator_ =
          this.screenshotSeries_(this.iterateStates(this.currentScreenId_));
      this.runIterator_();
    }

    makeScreenshotDeck() {
      this.hideDebugUI();
      this.commandIterator_ = this.screenshotSeries_(this.iterateScreens());
      this.runIterator_();
    }

    preProcessScreens() {
      KNOWN_SCREENS.forEach((screen, index) => {
        // Screen ordering
        screen.index = index;
        // Create a default state
        if (!screen.states) {
          const state = {
            id: 'default',
          };
          screen.states = [state];
        }
        // Assign "default" state for each screen
        if (!screen.defaultState) {
          screen.defaultState = screen.states[0].id;
        }
        screen.stateMap_ = {};
        // For each state fall back to screen data if state data is not defined.
        for (const state of screen.states) {
          if (!state.data) {
            state.data = screen.data;
          }
          screen.stateMap_[state.id] = state;
        }
      });
    }

    createLanguagePanel(parent) {
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
        new DebugButton(langPanel.content, pair[0], function(locale) {
          chrome.send('WelcomeScreen.setLocaleId', [locale]);
        }.bind(null, pair[1]));
      });
    }

    createToolsPanel(parent) {
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
      new DebugButton(panel.content, 'Toggle color mode', function() {
        chrome.send('debug.toggleColorMode');
      });
    }

    createScreensPanel(parent) {
      const panel = new ToolPanel(
          this.debuggerOverlay_, 'Screens', 'DebuggerPanelScreens');
      // List of screens will be created later, as not all screens
      // might be registered at this point.
      this.screensPanel = panel;
    }

    createStatesPanel(parent) {
      const panel = new ToolPanel(
          this.debuggerOverlay_, 'Screen States', 'DebuggerPanelStates');
      // List of states is rebuilt every time to reflect current screen.
      this.statesPanel = panel;
    }

    switchToScreen(screen) {
      this.triggerScreenState(screen.id, screen.defaultState);
    }

    triggerScreenState(screenId, stateId) {
      const screen = this.screenMap[screenId];
      const state = screen.stateMap_[stateId];
      var data = {};
      if (state.data) {
        data = state.data;
      }
      this.currentScreenId_ = screenId;
      this.lastScreenState_ = stateId;
      /** @suppress {visibility} */
      const displayManager = cr.ui.Oobe.instance_;
      cr.ui.Oobe.instance_.showScreen({id: screen.id, data: data});
      if (state.trigger) {
        state.trigger(displayManager.currentScreen);
      }
      this.lastScreen = displayManager.currentScreen;
    }

    createScreensList() {
      for (const screen of KNOWN_SCREENS) {
        this.screenMap[screen.id] = screen;
      }
      this.knownScreens = [];
      this.screenButtons = {};
      /** @suppress {visibility} */
      for (var id of cr.ui.Oobe.instance_.screens_) {
        if (id in this.screenMap) {
          const screenDef = this.screenMap[id];
          const screenElement = $(id);
          if (screenElement.listSteps &&
              typeof screenElement.listSteps === 'function') {
            if (screenDef.stateMap_['default']) {
              screenDef.states = [];
              screenDef.stateMap_ = {};
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
                trigger: (screen) => {
                  screen.setUIStep(step);
                },
              };
              screenDef.states.push(state);
              screenDef.stateMap_[state.id] = state;
            }
            if (screenDef.defaultState === 'default') {
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
      this.knownScreens = this.knownScreens.sort((a, b) => a.index - b.index);
      const content = this.screensPanel.content;
      this.knownScreens.forEach((screen) => {
        var name = screen.id;
        if (screen.suffix) {
          name = name + ' (' + screen.suffix + ')';
        }
        var button = new DebugButton(
            content, name, this.switchToScreen.bind(this, screen));
        button.element.classList.add('debug-button-' + screen.kind);
        this.screenButtons[screen.id] = button;
      });
    }

    refreshScreensPanel() {
      if (this.knownScreens === undefined) {
        this.createScreensList();
      }
      /** @suppress {visibility} */
      const displayManager = cr.ui.Oobe.instance_;
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

      const screen = this.screenMap[this.currentScreenId_];

      this.statesPanel.clearContent();
      for (const state of screen.states) {
        const button = new DebugButton(
            this.statesPanel.content, state.id,
            this.triggerScreenState.bind(
                this, this.currentScreenId_, state.id));
        if (state.id == this.lastScreenState_) {
          button.element.classList.add('debug-button-selected');
        }
      }

      this.statesPanel.show();
    }

    createCssStyle(name, styleSpec) {
      var style = document.createElement('style');
      style.type = 'text/css';
      style.innerHTML = '.' + name + ' {' + styleSpec + '}';
      document.getElementsByTagName('head')[0].appendChild(style);
    }

    register(element) {
      // Pre-process Screens data
      this.preProcessScreens();
      // Create CSS styles
      {
        this.createCssStyle('debugger-button', DEBUG_BUTTON_STYLE);
        this.createCssStyle('debugger-overlay', DEBUG_OVERLAY_STYLE);
        this.createCssStyle('debug-tool-panel', TOOL_PANEL_STYLE);
        this.createCssStyle('debug-tool-button', TOOL_BUTTON_STYLE);
        this.createCssStyle(
            'debug-button-selected', TOOL_BUTTON_SELECTED_STYLE);
        this.createCssStyle('debug-button-normal', SCREEN_BUTTON_STYLE_NORMAL);
        this.createCssStyle('debug-button-error', SCREEN_BUTTON_STYLE_ERROR);
        this.createCssStyle('debug-button-other', SCREEN_BUTTON_STYLE_OTHER);
        this.createCssStyle(
            'debug-button-unknown', SCREEN_BUTTON_STYLE_UNKNOWN);
      }
      {
        // Create UI Debugger button
        const button =
            /** @type {!HTMLElement} */ (document.createElement('div'));
        button.id = 'invokeDebuggerButton';
        button.className = 'debugger-button';
        button.textContent = 'Debug';
        button.addEventListener('click', this.toggleDebugUI.bind(this));

        this.debuggerButton_ = button;
      }
      {
        // Create base debugger panel.
        const overlay =
            /** @type {!HTMLElement} */ (document.createElement('div'));
        overlay.id = 'debuggerOverlay';
        overlay.className = 'debugger-overlay';
        overlay.setAttribute('hidden', true);
        this.debuggerOverlay_ = overlay;
      }
      this.createLanguagePanel(this.debuggerOverlay_);
      this.createScreensPanel(this.debuggerOverlay_);
      this.createStatesPanel(this.debuggerOverlay_);
      this.createToolsPanel(this.debuggerOverlay_);

      element.appendChild(this.debuggerButton_);
      element.appendChild(this.debuggerOverlay_);
    }
  }

  cr.addSingletonGetter(DebuggerUI);

  // #cr_define_end
  // Export
  return {
    DebuggerUI: DebuggerUI,
  };
});
