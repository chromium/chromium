// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/ash/common/assert.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {$} from '//resources/ash/common/util.js';
import {afterNextRender} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Oobe} from '../cr_ui.js';

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
    chrome.send('OobeTestApi.emulateDevicesForTesting');
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

  getPointingDeviceDetectedText() {
    return loadTimeData.getString('hidDetectionPointingDeviceConnected');
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
    this.personalCrButton = new PolymerElementApi(this, '#selfButton');
    this.enrollCrButton = new PolymerElementApi(this, '#enrollButton');
    this.enrollTriageCrButton =
        new PolymerElementApi(this, '#triageEnrollButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.enrollNextButton =
        new PolymerElementApi(this, '#enrollTriageNextButton');
  }

  /**
   * Presses enroll device button to select it in enroll triage step.
   */
  selectEnrollTriageButton() {
    this.enrollTriageCrButton.click();
  }

  /**
   * Presses for personal use button to select it.
   */
  selectPersonalUser() {
    this.personalCrButton.click();
  }

  /**
   * Presses for work button to select it.
   */
  selectForWork() {
    this.enrollCrButton.click();
  }

  /**
   * Presses next button in enroll-triage step.
   */
  clickEnrollNextButton() {
    this.enrollNextButton.click();
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
    return (
        this.isVisible() && !this.gaiaLoading.isVisible() &&
        this.signinFrame.isVisible() && this.gaiaDialog.isVisible());
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
    return loadTimeData.getBoolean('testapi_shouldSkipAssistant');
  }

  /**
   * Returns if the assistant screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return (
        this.isVisible() &&
        (this.valueProp.isVisible() || this.relatedInfo.isVisible()));
  }

  getSkipButtonName() {
    if (this.valueProp.isVisible()) {
      return this.valuePropSkipButtonText.element().textContent;
    }
    return loadTimeData.getString('assistantOptinNoThanksButton');
  }

  /**
   * Returns whether we currently show existing user flow.
   * @returns {boolean}
   */
  isPreviousUserFlowShown() {
    return this.relatedInfo.isVisible();
  }
}

class MarketingOptInScreenTester extends ScreenElementApi {
  constructor() {
    super('marketing-opt-in');
    this.accessibilityButton =
        new PolymerElementApi(this, '#marketing-opt-in-accessibility-button');
    this.accessibilityStep =
        new PolymerElementApi(this, '#finalAccessibilityPage');
    this.accessibilityToggle =
        new PolymerElementApi(this, '#a11yNavButtonToggle');
    this.marketingOptInGameDeviceTitle =
        new PolymerElementApi(this, '#marketingOptInGameDeviceTitle');
  }
  /** @override */
  shouldSkip() {
    return !loadTimeData.getBoolean('testapi_isBrandedBuild');
  }

  /**
   * Returns whether accessibility step is shown.
   * @returns {boolean}
   */
  isAccessibilityStepReadyForTesting() {
    return this.accessibilityStep.isVisible();
  }

  /**
   * Returns whether a11y button is visible on the marketing-opt-in screen.
   * @returns {boolean}
   */
  isAccessibilityButtonVisible() {
    return this.accessibilityButton.isVisible();
  }

  /**
   * Returns whether a11y toggle is on.
   * @returns {boolean}
   */
  isAccessibilityToggleOn() {
    return this.accessibilityToggle.element().checked;
  }

  /**
   * Returns whether gaming-specific title is visible.
   * @returns {boolean}
   */
  isMarketingOptInGameDeviceTitleVisible() {
    return this.marketingOptInGameDeviceTitle.isVisible();
  }

  /**
   * Returns a11y button name.
   * @returns {string}
   */
  getAccessibilityButtonName() {
    return loadTimeData.getString('marketingOptInA11yButtonLabel');
  }

  /**
   * Returns name of Done button on a11y page.
   * @returns {string}
   */
  getAccessibilityDoneButtonName() {
    return loadTimeData.getString('finalA11yPageDoneButtonTitle');
  }

  /**
   * Returns name of Get Started button.
   * @returns {string}
   */
  getGetStartedButtonName() {
    return loadTimeData.getString('marketingOptInScreenAllSet');
  }

  /**
   * Returns gaming-specific title.
   * @returns {string}
   */
  getCloudGamingDeviceTitle() {
    return loadTimeData.getString('marketingOptInScreenGameDeviceTitle');
  }
}

class ThemeSelectionScreenTester extends ScreenElementApi {
  constructor() {
    super('theme-selection');
    this.themeRadioButton = new PolymerElementApi(this, '#theme');
    this.lightThemeButton = new PolymerElementApi(this, '#lightThemeButton');
    this.darkThemeButton = new PolymerElementApi(this, '#darkThemeButton');
    this.autoThemeButton = new PolymerElementApi(this, '#autoThemeButton');
    this.textHeader = new PolymerElementApi(this, '#theme-selection-title');
  }

  /**
   * Returns if the Theme Selection Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return (
        this.isVisible() && this.lightThemeButton.isVisible() &&
        this.darkThemeButton.isVisible() && this.autoThemeButton.isVisible());
  }

  /**
   * Presses light theme button to select it.
   */
  selectLightTheme() {
    this.lightThemeButton.click();
  }

  /**
   * Presses dark theme button to select it.
   */
  selectDarkTheme() {
    this.darkThemeButton.click();
  }

  /**
   * Presses auto theme button to select it.
   */
  selectAutoTheme() {
    this.autoThemeButton.click();
  }

  /**
   * Finds which theme is selected.
   * @returns {string}
   */
  getNameOfSelectedTheme() {
    return this.themeRadioButton.element().selected;
  }

  /**
   * Retrieves computed color of the screen header. This value will be used to
   * determine screen's color mode.
   * @returns {string}
   */
  getHeaderTextColor() {
    return window.getComputedStyle(this.textHeader.element()).color;
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
    afterNextRender(assert(this.element()), () => {
      this.confirmPasswordInput.typeInto(password);
      afterNextRender(assert(this.element()), () => {
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
    return (
        this.isVisible() && this.signInFrame.isVisible() &&
        this.nextButton.isVisible());
  }
}

class EnrollmentAttributeStep extends PolymerElementApi {
  constructor(parent) {
    super(parent, '#step-attribute-prompt');
    this.skipButton = new PolymerElementApi(parent, '#attributesSkip');
  }

  isReadyForTesting() {
    return this.isVisible() && this.skipButton.isVisible();
  }

  clickSkip() {
    return this.skipButton.click();
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
    this.attributeStep = new EnrollmentAttributeStep(this);
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
    this.errorTitle = new PolymerElementApi(this, '#error-title');
    this.errorSubtitle = new PolymerElementApi(this, '#error-subtitle');
  }

  /**
   * Returns if the Error Screen is ready for test interaction.
   * @return {boolean}
   */
  isReadyForTesting() {
    return this.isVisible();
  }

  /**
   *
   * Returns if offline link is visible.
   * @return {boolean}
   */
  isOfflineLinkVisible() {
    return this.offlineLink.isVisible();
  }

  /**
   * Returns error screen message title.
   * @return {string}
   */
  getErrorTitle() {
    // If screen is not visible always return empty title.
    if (!this.isVisible()) {
      return '';
    }

    return this.errorTitle.element().innerText.trim();
  }

  /**
   * Returns error screen subtitle. Includes all visible error messages.
   * @return {string}
   */
  getErrorReasons() {
    // If screen is not visible always return empty reasons.
    if (!this.isVisible()) {
      return '';
    }

    return this.errorSubtitle.element().innerText.trim();
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

class GuestTosScreenTester extends ScreenElementApi {
  constructor() {
    super('guest-tos');
    this.loadedStep = new PolymerElementApi(this, '#loaded');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');

    this.googleEulaDialog = new PolymerElementApi(this, '#googleEulaDialog');
    this.crosEulaDialog = new PolymerElementApi(this, '#crosEulaDialog');

    this.googleEulaDialogLink = new PolymerElementApi(this, '#googleEulaLink');
    this.crosEulaDialogLink = new PolymerElementApi(this, '#crosEulaLink');
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

  /** @return {string} */
  getEulaButtonName() {
    return loadTimeData.getString('guestTosOk');
  }

  /** @return {boolean} */
  isGoogleEulaDialogShown() {
    return this.googleEulaDialog.isVisible();
  }

  /** @return {boolean} */
  isCrosEulaDialogShown() {
    return this.crosEulaDialog.isVisible();
  }

  /** @return {string} */
  getGoogleEulaLinkName() {
    return this.googleEulaDialogLink.element().text.trim();
  }

  /** @return {string} */
  getCrosEulaLinkName() {
    return this.crosEulaDialogLink.element().text.trim();
  }
}

class GestureNavigationScreenTester extends ScreenElementApi {
  constructor() {
    super('gesture-navigation');
  }

  /** @return {string} */
  getNextButtonName() {
    return loadTimeData.getString('gestureNavigationIntroNextButton');
  }

  /** @return {string} */
  getSkipButtonName() {
    return loadTimeData.getString('gestureNavigationIntroSkipButton');
  }
}

class ConsolidatedConsentScreenTester extends ScreenElementApi {
  constructor() {
    super('consolidated-consent');
    this.loadedStep = new PolymerElementApi(this, '#loadedDialog');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');
    this.readMoreButton =
        new PolymerElementApi(this.loadedStep, '#readMoreButton');
    this.recoveryToggle = new PolymerElementApi(this, '#recoveryOptIn');

    this.googleEulaDialog = new PolymerElementApi(this, '#googleEulaDialog');
    this.crosEulaDialog = new PolymerElementApi(this, '#crosEulaDialog');
    this.arcTosDialog = new PolymerElementApi(this, '#arcTosDialog');
    this.privacyPolicyDialog =
        new PolymerElementApi(this, '#privacyPolicyDialog');

    this.googleEulaLink = new PolymerElementApi(this, '#googleEulaLink');
    this.crosEulaLink = new PolymerElementApi(this, '#crosEulaLink');
    this.arcTosLink = new PolymerElementApi(this, '#arcTosLink');
    this.privacyPolicyLink = new PolymerElementApi(this, '#privacyPolicyLink');
  }

  /** @override */
  shouldSkip() {
    return loadTimeData.getBoolean('testapi_shouldSkipConsolidatedConsent');
  }

  /** @return {boolean} */
  isReadyForTesting() {
    return this.isVisible() && this.loadedStep.isVisible();
  }

  /** @return {boolean} */
  isReadMoreButtonShown() {
    // The read more button is inside a <dom-if> element, if it's hidden, the
    // element would be removed entirely from dom, so we need to check if the
    // element exists before checking if it's visible.
    return (
        this.readMoreButton.element() != null &&
        this.readMoreButton.isVisible());
  }

  /** @return {string} */
  getNextButtonName() {
    return loadTimeData.getString('consolidatedConsentAcceptAndContinue');
  }

  /**
   * Enable the toggle which controls whether the user opted-in the the
   * cryptohome recovery feature.
   */
  enableRecoveryToggle() {
    this.recoveryToggle.element().checked = true;
  }

  /** @return {string} */
  getEulaOkButtonName() {
    return loadTimeData.getString('consolidatedConsentOK');
  }

  /** @return {boolean} */
  isGoogleEulaDialogShown() {
    return this.googleEulaDialog.isVisible();
  }

  /** @return {boolean} */
  isCrosEulaDialogShown() {
    return this.crosEulaDialog.isVisible();
  }

  /** @return {boolean} */
  isArcTosDialogShown() {
    return this.arcTosDialog.isVisible();
  }

  /** @return {boolean} */
  isPrivacyPolicyDialogShown() {
    return this.privacyPolicyDialog.isVisible();
  }

  /** @return {string} */
  getGoogleEulaLinkName() {
    return this.googleEulaLink.element().text.trim();
  }

  /** @return {string} */
  getCrosEulaLinkName() {
    return this.crosEulaLink.element().text.trim();
  }

  /** @return {string} */
  getArcTosLinkName() {
    return this.arcTosLink.element().text.trim();
  }

  /** @return {string} */
  getPrivacyPolicyLinkName() {
    return this.privacyPolicyLink.element().text.trim();
  }

  /**
   * Click `accept` button to go to the next screen.
   */
  clickAcceptButton() {
    this.nextButton.element().click();
  }
}

class SmartPrivacyProtectionScreenTester extends ScreenElementApi {
  constructor() {
    super('smart-privacy-protection');
    this.noThanks = new PolymerElementApi(this, '#noThanksButton');
  }

  /** @override */
  shouldSkip() {
    return !loadTimeData.getBoolean('testapi_isHPSEnabled');
  }

  /** @return {boolean} */
  isReadyForTesting() {
    return this.isVisible() && this.noThanks.isVisible();
  }

  /** @return {string} */
  getNoThanksButtonName() {
    return loadTimeData.getString('smartPrivacyProtectionTurnOffButton');
  }
}

class CryptohomeRecoverySetupScreenTester extends ScreenElementApi {
  constructor() {
    super('cryptohome-recovery-setup');
  }
}

class LocalPasswordSetupScreenTester extends ScreenElementApi {
  constructor() {
    super('local-password-setup');
    this.passwordInput = new PolymerElementApi(this, '#passwordInput');
    this.firstInput = new TextFieldApi(this.passwordInput, '#firstInput');
    this.confirmInput = new TextFieldApi(this.passwordInput, '#confirmInput');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.doneDialog = new PolymerElementApi(this, '#doneDialog');
    this.doneButton = new PolymerElementApi(this, '#doneButton');
  }

  /** @return {boolean} */
  isReadyForTesting() {
    return this.isVisible() && this.firstInput.isVisible() &&
        this.confirmInput.isVisible();
  }

  enterPassword(password) {
    this.firstInput.typeInto(password);
    afterNextRender(assert(this.element()), () => {
      this.confirmInput.typeInto(password);
      afterNextRender(assert(this.element()), () => {
        this.nextButton.click();
      });
    });
  }

  /** @return {boolean} */
  isDone() {
    return this.doneDialog.isVisible();
  }

  clickDone() {
    this.doneButton.click();
  }
}

class GaiaInfoScreenTester extends ScreenElementApi {
  constructor() {
    super('gaia-info');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }
}

class ConsumerUpdateScreenTester extends ScreenElementApi {
  constructor() {
    super('consumer-update');
    this.skipButton = new PolymerElementApi(this, '#skipButton');
  }

  clickSkip() {
    this.skipButton.click();
  }
}

class ChoobeScreenTester extends ScreenElementApi {
  constructor() {
    super('choobe');
    this.skipButton = new PolymerElementApi(this, '#skipButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.choobeScreensList = new PolymerElementApi(this, '#screensList');
    this.drivePinningScreenButton = new PolymerElementApi(
        this.choobeScreensList, '#cr-button-drive-pinning');
  }

  isReadyForTesting() {
    return this.isVisible();
  }

  clickDrivePinningScreen() {
    this.drivePinningScreenButton.click();
  }

  isDrivePinningScreenVisible() {
    return this.drivePinningScreenButton.element() &&
        this.drivePinningScreenButton.isVisible();
  }

  isDrivePinningScreenChecked() {
    return !!this.drivePinningScreenButton.element().getAttribute('checked');
  }

  clickNext() {
    this.nextButton.click();
  }

  clickSkip() {
    this.skipButton.click();
  }
}

class ChoobeDrivePinningScreenTester extends ScreenElementApi {
  constructor() {
    super('drive-pinning');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.drivePinningToggle =
        new PolymerElementApi(this, '#drivePinningToggle');
    this.drivePinningSpaceInformation =
        new PolymerElementApi(this, '#spaceInformation');
  }

  isReadyForTesting() {
    return this.isVisible() && this.drivePinningToggle.isVisible() &&
        this.drivePinningSpaceInformation.isVisible();
  }

  toggleFileSync() {
    this.drivePinningToggle.click();
  }

  isFileSyncEnabled() {
    return !!this.drivePinningToggle.element().checked;
  }

  getSpaceInformationString() {
    return this.drivePinningSpaceInformation.element().innerText;
  }

  clickNext() {
    this.nextButton.click();
  }
}

export class OobeApiProvider {
  constructor() {
    this.screens = {
      HIDDetectionScreen: new HIDDetectionScreenTester(),
      WelcomeScreen: new WelcomeScreenTester(),
      NetworkScreen: new NetworkScreenTester(),
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
      ThemeSelectionScreen: new ThemeSelectionScreenTester(),
      GestureNavigation: new GestureNavigationScreenTester(),
      ConsolidatedConsentScreen: new ConsolidatedConsentScreenTester(),
      SmartPrivacyProtectionScreen: new SmartPrivacyProtectionScreenTester(),
      CryptohomeRecoverySetupScreen: new CryptohomeRecoverySetupScreenTester(),
      LocalPasswordSetupScreen: new LocalPasswordSetupScreenTester(),
      GaiaInfoScreen: new GaiaInfoScreenTester(),
      ConsumerUpdateScreen: new ConsumerUpdateScreenTester(),
      ChoobeScreen: new ChoobeScreenTester(),
      ChoobeDrivePinningScreen: new ChoobeDrivePinningScreenTester(),
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

    this.getBrowseAsGuestButtonName = function() {
      return loadTimeData.getString('testapi_browseAsGuest');
    };

    this.getCurrentScreenName = function() {
      return Oobe.getInstance().currentScreen.id.trim();
    };

    this.getCurrentScreenStep = function() {
      const step = Oobe.getInstance().currentScreen.getAttribute('multistep');
      if (step === null) {
        return 'default';
      }
      return step.trim();
    };

    /**
     * Returns currently displayed OOBE dialog HTML element.
     * @returns {Object}
     */
    this.getOobeActiveDialog = function() {
      const adaptiveDialogs =
          Oobe.getInstance().currentScreen.shadowRoot.querySelectorAll(
              'oobe-adaptive-dialog');
      for (const dialog of adaptiveDialogs) {
        // Only one adaptive dialog could be shown at the same time.
        if (!dialog.hidden) {
          return dialog;
        }
      }

      // If we didn't find any active adaptive dialog we might currently display
      // a loading dialog, a special wrapper over oobe-adaptive-dialog to show
      // loading process.
      // In this case we can try to fetch all loading dialogs attached to the
      // current screen and check their internal adaptive dialogs.
      const loadingDialogs =
          Oobe.getInstance().currentScreen.shadowRoot.querySelectorAll(
              'oobe-loading-dialog');
      for (const dialog of loadingDialogs) {
        if (!dialog.hidden) {
          return dialog.shadowRoot.querySelector('oobe-adaptive-dialog');
        }
      }

      return null;
    };

    /**
     * Returns array of the slot HTML elements with a given slot name.
     * @param {string} slotName
     * @returns {!Array<!Element>}
     */
    this.findActiveOobeDialogSlotsByName = function(slotName) {
      const dialog = this.getOobeActiveDialog();
      if (dialog === null) {
        return [];
      }

      if (dialog.children === undefined || dialog.children == null) {
        return [];
      }

      // There are cases when we have different element with the same slot, so
      // we must find all of them for a given adaptive dialog.
      const result = [];

      for (const child of dialog.children) {
        if (child.hidden) {
          continue;
        }

        const slot = child.slot;
        if (slot === undefined) {
          continue;
        }

        if (typeof slot !== 'string') {
          continue;
        }

        if (slot.toLowerCase().trim() === slotName.toLowerCase().trim()) {
          result.push(child);
        }
      }

      return result;
    };

    /**
     * Concatenates innerText of all slots with a given name inside a currently
     * displayed OOBE dialog.
     * @param {string} slotName
     * @returns {string}
     */
    this.combineTextOfAdaptiveDialogSlots = function(slotName) {
      const slots = this.findActiveOobeDialogSlotsByName(slotName);
      let result = '';

      // innerText should be sufficient as it contains only visible text, no
      // need to manually traverse a DOM tree and check all child elements.
      for (const slot of slots) {
        result = result.concat(slot.innerText.trim().concat('\n'));
      }

      return result;
    };

    /**
     * Returns text inside displayed title slots.
     * @returns {string}
     */
    this.getOobeActiveDialogTitleText = function() {
      return this.combineTextOfAdaptiveDialogSlots('title');
    };

    /**
     * Returns text inside displayed subtitle slots.
     * @returns {string}
     */
    this.getOobeActiveDialogSubtitleText = function() {
      return this.combineTextOfAdaptiveDialogSlots('subtitle');
    };

    /**
     * Returns text inside displayed content slots.
     * @returns {string}
     */
    this.getOobeActiveDialogContentText = function() {
      return this.combineTextOfAdaptiveDialogSlots('content');
    };
  }
}
