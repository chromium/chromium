// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {afterNextRender, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {$} from 'chrome://resources/js/util.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// clang-format on

/**
 * @fileoverview Common testing utils methods used for OOBE tast tests.
 */

class TestElementApi {
  /**
   * Returns HTMLElement with $$ support.
   * @return {HTMLElement}
   */
  element() {
    throw new Error('element() should be defined!');
  }

  /**
   * Returns whether the element is visible.
   * @return {boolean}
   */
  isVisible() {
    return !this.element().hidden;
  }

  /**
   * Returns whether the element is enabled.
   * @return {boolean}
   */
  isEnabled() {
    return !this.element().disabled;
  }
}

class ScreenElementApi extends TestElementApi {
  constructor(id) {
    super();
    this.id = id;
    this.nextButton = undefined;
  }

  /** @override */
  element() {
    return $(this.id);
  }

  /**
   * Click on the primary action button ("Next" usually).
   */
  clickNext() {
    assert(this.nextButton);
    this.nextButton.click();
  }

  /**
   * Returns whether the screen should be skipped.
   * @return {boolean}
   */
  shouldSkip() {
    return false;
  }
}

class PolymerElementApi extends TestElementApi {
  constructor(parent, query) {
    super();
    this.parent = parent;
    this.query = query;
  }

  /** @override */
  element() {
    assert(this.parent.element());
    return this.parent.element().shadowRoot.querySelector(this.query);
  }

  /**
   * Assert element is visible/enabled and click on the element.
   */
  click() {
    assert(this.isVisible());
    assert(this.isEnabled());
    this.element().click();
  }
}

class TextFieldApi extends PolymerElementApi {
  constructor(parent, query) {
    super(parent, query);
  }

  /**
   * Assert element is visible/enabled and fill in the element with a value.
   * @param {string} value
   */
  typeInto(value) {
    assert(this.isVisible());
    assert(this.isEnabled());
    this.element().value = value;
    this.element().dispatchEvent(new Event('input'));
    this.element().dispatchEvent(new Event('change'));
  }
}

class HIDDetectionScreenTester extends ScreenElementApi {
  constructor() {
    super('hid-detection');
    this.nextButton = new PolymerElementApi(this, '#hid-continue-button');
  }

  // Must be called to enable the next button
  emulateDevicesConnected() {
    chrome.send('HIDDetectionScreen.emulateDevicesConnectedForTesting');
  }

  touchscreenDetected() {
    // Touchscreen entire row is only visible when touchscreen is detected.
    const touchscreenRow =
        new PolymerElementApi(this, '#hid-touchscreen-entry');
    return touchscreenRow.isVisible();
  }

  mouseDetected() {
    const mouseTickIcon = new PolymerElementApi(this, '#mouse-tick');
    return mouseTickIcon.isVisible();
  }

  keyboardDetected() {
    const keyboardTickIcon = new PolymerElementApi(this, '#keyboard-tick');
    return keyboardTickIcon.isVisible();
  }

  getKeyboardNotDetectedText() {
    return loadTimeData.getString('hidDetectionKeyboardSearching');
  }

  getMouseNotDetectedText() {
    return loadTimeData.getString('hidDetectionMouseSearching');
  }

  getUsbKeyboardDetectedText() {
    return loadTimeData.getString('hidDetectionUSBKeyboardConnected');
  }

  getNextButtonName() {
    return loadTimeData.getString('hidDetectionContinue');
  }

  canClickNext() {
    return this.nextButton.isEnabled();
  }
}

class WelcomeScreenTester extends ScreenElementApi {
  constructor() {
    super('connect');
    this.demoModeConfirmationDialog =
        new PolymerElementApi(this, '#demoModeConfirmationDialog');
  }

  /** @override */
  clickNext() {
    if (!this.nextButton) {
      const mainStep = new PolymerElementApi(this, '#welcomeScreen');
      this.nextButton = new PolymerElementApi(mainStep, '#getStarted');
    }

    assert(this.nextButton);
    this.nextButton.click();
  }
  getDemoModeOkButtonName() {
    return loadTimeData.getString('enableDemoModeDialogConfirm');
  }
}

class NetworkScreenTester extends ScreenElementApi {
  constructor() {
    super('network-selection');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  /** @override */
  shouldSkip() {
    return loadTimeData.getBoolean('testapi_shouldSkipNetworkFirstShow');
  }
}

class EulaScreenTester extends ScreenElementApi {
  constructor() {
    super('oobe-eula-md');
    this.eulaStep = new PolymerElementApi(this, '#eulaDialog');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');
  }

  /** @override */
  shouldSkip() {
    // Eula screen should skipped on non-branded build and on CfM devices.
    return loadTimeData.getBoolean('testapi_shouldSkipEula');
  }

  /**
   * Returns if the EULA Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.eulaStep.isVisible() &&
        this.nextButton.isVisible();
  }

  getNextButtonName() {
    return loadTimeData.getString('oobeEulaAcceptAndContinueButtonText');
  }
}

class UpdateScreenTester extends ScreenElementApi {
  constructor() {
    super('oobe-update');
  }
}

class EnrollmentScreenTester extends ScreenElementApi {
  constructor() {
    super('enterprise-enrollment');
  }
}

class UserCreationScreenTester extends ScreenElementApi {
  constructor() {
    super('user-creation');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }
}

class GaiaScreenTester extends ScreenElementApi {
  constructor() {
    super('gaia-signin');
    this.signinFrame = new PolymerElementApi(this, '#signin-frame-dialog');
    this.gaiaDialog = new PolymerElementApi(this.signinFrame, '#gaiaDialog');
    this.gaiaLoading = new PolymerElementApi(this, '#step-loading');
  }

  /**
   * Returns if the Gaia Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && !this.gaiaLoading.isVisible() &&
        this.signinFrame.isVisible() && this.gaiaDialog.isVisible();
  }
}

class SyncScreenTester extends ScreenElementApi {
  constructor() {
    super('sync-consent');
    this.loadedStep = new PolymerElementApi(this, '#syncConsentOverviewDialog');
  }

  /**
   * Returns if the Sync Consent Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.loadedStep.isVisible();
  }
}

class FingerprintScreenTester extends ScreenElementApi {
  constructor() {
    super('fingerprint-setup');
  }
  /** @override */
  shouldSkip() {
    return !loadTimeData.getBoolean('testapi_isFingerprintSupported');
  }
}

class AssistantScreenTester extends ScreenElementApi {
  constructor() {
    super('assistant-optin-flow');
    this.mainElement = new PolymerElementApi(this, '#card');
    this.valueProp = new PolymerElementApi(this.mainElement, '#valueProp');
    this.valuePropSkipButtonText =
        new PolymerElementApi(this.valueProp, '#skip-button-text');
    this.relatedInfo = new PolymerElementApi(this.mainElement, '#relatedInfo');
  }
  /** @override */
  shouldSkip() {
    return !loadTimeData.getBoolean('testapi_isLibAssistantEnabled');
  }

  /**
   * Returns if the assistant screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() &&
        (this.valueProp.isVisible() || this.relatedInfo.isVisible());
  }

  getSkipButtonName() {
    if (this.valueProp.isVisible()) {
      return this.valuePropSkipButtonText.element().textContent;
    }
    return loadTimeData.getString('assistantOptinNoThanksButton');
  }
}

class MarketingOptInScreenTester extends ScreenElementApi {
  constructor() {
    super('marketing-opt-in');
  }
  /** @override */
  shouldSkip() {
    return !loadTimeData.getBoolean('testapi_isBrandedBuild');
  }
}

class ConfirmSamlPasswordScreenTester extends ScreenElementApi {
  constructor() {
    super('saml-confirm-password');
    this.passwordInput = new TextFieldApi(this, '#passwordInput');
    this.confirmPasswordInput = new TextFieldApi(this, '#confirmPasswordInput');
    this.nextButton = new PolymerElementApi(this, '#next');
  }

  /**
   * Enter password input fields with password value and submit the form.
   * @param {string} password
   */
  enterManualPasswords(password) {
    this.passwordInput.typeInto(password);
    Polymer.RenderStatus.afterNextRender(assert(this.element()), () => {
      this.confirmPasswordInput.typeInto(password);
      Polymer.RenderStatus.afterNextRender(assert(this.element()), () => {
        this.clickNext();
      });
    });
  }
}

class PinSetupScreenTester extends ScreenElementApi {
  constructor() {
    super('pin-setup');
    this.skipButton = new PolymerElementApi(this, '#setupSkipButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.doneButton = new PolymerElementApi(this, '#doneButton');
    this.backButton = new PolymerElementApi(this, '#backButton');
    const pinSetupKeyboard = new PolymerElementApi(this, '#pinKeyboard');
    const pinKeyboard = new PolymerElementApi(pinSetupKeyboard, '#pinKeyboard');
    this.pinField = new TextFieldApi(pinKeyboard, '#pinInput');
    this.pinButtons = {};
    for (let i = 0; i <= 9; i++) {
      this.pinButtons[i.toString()] =
          new PolymerElementApi(pinKeyboard, '#digitButton' + i.toString());
    }
  }

  /**
   * Enter PIN into PINKeyboard input field, without submitting.
   * @param {string} pin
   */
  enterPin(pin) {
    this.pinField.typeInto(pin);
  }

  /**
   * Presses a single digit button in the PIN keyboard.
   * @param {string} digit String with single digit to be clicked on.
   */
  pressPinDigit(digit) {
    this.pinButtons[digit].click();
  }

  /**
   * @return {string}
   */
  getSkipButtonName() {
    return loadTimeData.getString('discoverPinSetupSkip');
  }

  /** @return {boolean} */
  isInTabletMode() {
    return loadTimeData.getBoolean('testapi_isOobeInTabletMode');
  }
}

class EnrollmentSignInStep extends PolymerElementApi {
  constructor(parent) {
    super(parent, '#step-signin');
    this.signInFrame = new PolymerElementApi(this, '#signin-frame');
    this.nextButton = new PolymerElementApi(this, '#primary-action-button');
  }

  /**
   * Returns if the Enrollment Signing step is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.signInFrame.isVisible() &&
        this.nextButton.isVisible();
  }
}

class EnrollmentSuccessStep extends PolymerElementApi {
  constructor(parent) {
    super(parent, '#step-success');
    this.nextButton = new PolymerElementApi(parent, '#successDoneButton');
  }

  /**
   * Returns if the Enrollment Success step is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.nextButton.isVisible();
  }

  clickNext() {
    this.nextButton.click();
  }
}

class EnrollmentErrorStep extends PolymerElementApi {
  constructor(parent) {
    super(parent, '#step-error');
    this.retryButton = new PolymerElementApi(parent, '#errorRetryButton');
    this.errorMsgContainer = new PolymerElementApi(parent, '#errorMsg');
  }

  /**
   * Returns if the Enrollment Error step is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.errorMsgContainer.isVisible();
  }

  /**
   * Returns if enterprise enrollment can be retried.
   * @return {boolean}
   */
  canRetryEnrollment() {
    return this.retryButton.isVisible() && this.retryButton.isEnabled();
  }

  /**
   * Click the Retry button on the enrollment error screen.
   */
  clickRetryButton() {
    assert(this.canRetryEnrollment());
    this.retryButton.click();
  }

  /**
   * Returns the error text shown on the enrollment error screen.
   * @return {string}
   */
  getErrorMsg() {
    assert(this.isReadyForTesting());
    return this.errorMsgContainer.element().innerText;
  }
}

class EnterpriseEnrollmentScreenTester extends ScreenElementApi {
  constructor() {
    super('enterprise-enrollment');
    this.signInStep = new EnrollmentSignInStep(this);
    this.successStep = new EnrollmentSuccessStep(this);
    this.errorStep = new EnrollmentErrorStep(this);
    this.enrollmentInProgressDlg = new PolymerElementApi(this, '#step-working');
  }

  /**
   * Returns if enrollment is in progress.
   * @return {boolean}
   */
  isEnrollmentInProgress() {
    return this.enrollmentInProgressDlg.isVisible();
  }
}

class OfflineLoginScreenTester extends ScreenElementApi {
  constructor() {
    super('offline-login');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  /**
   * Returns if the Offline Login Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.nextButton.isVisible();
  }

  /**
   * Returns the email field name on the Offline Login Screen.
   * @return {string}
   */
  getEmailFieldName() {
    return loadTimeData.getString('offlineLoginEmail');
  }

  /**
   * Returns the password field name on the Offline Login Screen.
   * @return {string}
   */
  getPasswordFieldName() {
    return loadTimeData.getString('offlineLoginPassword');
  }

  /**
   * Returns the next button name on the Offline Login Screen.
   * @return {string}
   */
  getNextButtonName() {
    return loadTimeData.getString('offlineLoginNextBtn');
  }
}

class ErrorScreenTester extends ScreenElementApi {
  constructor() {
    super('error-message');
    this.offlineLink = new PolymerElementApi(this, '#error-offline-login-link');
  }

  /**
   * Returns if the Error Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible() && this.offlineLink.isVisible();
  }

  /**
   * Click the sign in as an existing user Link.
   */
  clickSignInAsExistingUserLink() {
    return this.offlineLink.click();
  }
}

class DemoPreferencesScreenTester extends ScreenElementApi {
  constructor() {
    super('demo-preferences');
  }

  getDemoPreferencesNextButtonName() {
    return loadTimeData.getString('demoPreferencesNextButtonLabel');
  }
}

class ArcTosScreenTester extends ScreenElementApi {
  constructor() {
    super('arc-tos');
  }

  // Note that the Accept Button text key is different depending on whether
  // the device in Demo Mode setup. Key for non-demo setup is
  // "arcTermsOfServiceAcceptButton"
  getArcTosDemoModeAcceptButtonName() {
    return loadTimeData.getString('arcTermsOfServiceAcceptAndContinueButton');
  }
}

class OobeApiProvider {
  constructor() {
    this.screens = {
      HIDDetectionScreen: new HIDDetectionScreenTester(),
      WelcomeScreen: new WelcomeScreenTester(),
      NetworkScreen: new NetworkScreenTester(),
      EulaScreen: new EulaScreenTester(),
      UpdateScreen: new UpdateScreenTester(),
      EnrollmentScreen: new EnrollmentScreenTester(),
      UserCreationScreen: new UserCreationScreenTester(),
      GaiaScreen: new GaiaScreenTester(),
      SyncScreen: new SyncScreenTester(),
      FingerprintScreen: new FingerprintScreenTester(),
      AssistantScreen: new AssistantScreenTester(),
      MarketingOptInScreen: new MarketingOptInScreenTester(),
      ConfirmSamlPasswordScreen: new ConfirmSamlPasswordScreenTester(),
      PinSetupScreen: new PinSetupScreenTester(),
      EnterpriseEnrollmentScreen: new EnterpriseEnrollmentScreenTester(),
      GuestTosScreen: new GuestTosScreenTester(),
      ErrorScreen: new ErrorScreenTester(),
      OfflineLoginScreen: new OfflineLoginScreenTester(),
      DemoPreferencesScreen: new DemoPreferencesScreenTester(),
      ArcTosScreen: new ArcTosScreenTester(),
    };

    this.loginWithPin = function(username, pin) {
      chrome.send('OobeTestApi.loginWithPin', [username, pin]);
    };

    this.advanceToScreen = function(screen) {
      chrome.send('OobeTestApi.advanceToScreen', [screen]);
    };

    this.skipToLoginForTesting = function() {
      chrome.send('OobeTestApi.skipToLoginForTesting');
    };

    this.skipPostLoginScreens = function() {
      chrome.send('OobeTestApi.skipPostLoginScreens');
    };

    this.loginAsGuest = function() {
      chrome.send('OobeTestApi.loginAsGuest');
    };

    this.showGaiaDialog = function() {
      chrome.send('OobeTestApi.showGaiaDialog');
    };
  }
}

class GuestTosScreenTester extends ScreenElementApi {
  constructor() {
    super('guest-tos');
    this.loadedStep = new PolymerElementApi(this, '#loaded');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');
  }

  /** @override */
  shouldSkip() {
    return loadTimeData.getBoolean('testapi_shouldSkipGuestTos');
  }

  /** @return {boolean} */
  isReadyForTesting() {
    return this.isVisible() && this.loadedStep.isVisible();
  }

  /** @return {string} */
  getNextButtonName() {
    return loadTimeData.getString('guestTosAccept');
  }
}

window.OobeAPI = new OobeApiProvider();
