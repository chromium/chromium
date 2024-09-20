// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrRadioGroupElement} from '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import {CrToggleElement} from '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {assert} from '//resources/js/assert.js';
import {sendWithPromise} from '//resources/js/cr.js';
import {afterNextRender} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeA11yOption} from '../components/oobe_a11y_option.js';
import {Oobe} from '../cr_ui.js';

/**
 * @fileoverview Common testing utils methods used for OOBE tast tests.
 */

class TestElementApi {
  /**
   * Returns HTMLElement with $$ support.
   */
  element(): HTMLElement|null {
    throw new Error('element() should be defined!');
  }

  /**
   * Returns whether the element is visible.
   */
  isVisible(): boolean {
    const el = this.element();
    return !!el && !el.hidden;
  }

  /**
   * Returns whether the element is enabled.
   */
  isEnabled(): boolean {
    const el = this.element();
    if (!!el && 'disabled' in el) {
      return !el.disabled;
    }
    return true;
  }
}

class ScreenElementApi extends TestElementApi {
  protected id: string;
  protected nextButton: PolymerElementApi|undefined;

  constructor(id: string) {
    super();
    this.id = id;
    this.nextButton = undefined;
  }

  override element(): HTMLElement|null {
    const el = document.getElementById(this.id);
    assert(
        el instanceof HTMLElement, this.id + ' should be a valid HTMLElement');
    return el;
  }

  /**
   * Click on the primary action button ("Next" usually).
   */
  clickNext(): void {
    assert(this.nextButton);
    this.nextButton.click();
  }

  /**
   * Returns whether the screen should be skipped.
   */
  shouldSkip(): boolean {
    return false;
  }
}

class PolymerElementApi extends TestElementApi {
  private parent: TestElementApi;
  private query: string;

  constructor(parent: TestElementApi, query: string) {
    super();
    this.parent = parent;
    this.query = query;
  }

  override element(): HTMLElement|null {
    const parentElement = this.parent.element();
    assert(
        parentElement instanceof HTMLElement,
        'Parent must be a valid HTMLElement');
    const polymerElement = parentElement.shadowRoot?.querySelector(this.query);
    return polymerElement instanceof HTMLElement ? polymerElement : null;
  }

  /**
   * Assert element is visible/enabled and click on the element.
   */
  click(): void {
    assert(this.isVisible(), 'Element must be visible before click');
    assert(this.isEnabled(), 'Element must be enabled before click');
    const el = this.element();
    assert(el instanceof HTMLElement);
    el.click();
  }
}

class TextFieldApi extends PolymerElementApi {
  constructor(parent: TestElementApi, query: string) {
    super(parent, query);
  }

  /**
   * Assert element is visible/enabled and fill in the element with a value.
   */
  typeInto(value: string): void {
    assert(this.isVisible(), 'Text field must be visible');
    assert(this.isEnabled(), 'Text field must be enabled');
    const el = this.element();
    assert(el instanceof HTMLElement && 'value' in el);
    el.value = value;
    el.dispatchEvent(new Event('input'));
    el.dispatchEvent(new Event('change'));
  }
}

class HidDetectionScreenTester extends ScreenElementApi {
  constructor() {
    super('hid-detection');
    this.nextButton = new PolymerElementApi(this, '#hid-continue-button');
  }

  touchscreenDetected(): boolean {
    // Touchscreen entire row is only visible when touchscreen is detected.
    const touchscreenRow =
        new PolymerElementApi(this, '#hid-touchscreen-entry');
    return touchscreenRow.isVisible();
  }

  mouseDetected(): boolean {
    const mouseTickIcon = new PolymerElementApi(this, '#mouse-tick');
    return mouseTickIcon.isVisible();
  }

  keyboardDetected(): boolean {
    const keyboardTickIcon = new PolymerElementApi(this, '#keyboard-tick');
    return keyboardTickIcon.isVisible();
  }

  getKeyboardNotDetectedText(): string {
    return loadTimeData.getString('hidDetectionKeyboardSearching');
  }

  getMouseNotDetectedText(): string {
    return loadTimeData.getString('hidDetectionMouseSearching');
  }

  getUsbKeyboardDetectedText(): string {
    return loadTimeData.getString('hidDetectionUSBKeyboardConnected');
  }

  getPointingDeviceDetectedText(): string {
    return loadTimeData.getString('hidDetectionPointingDeviceConnected');
  }

  getNextButtonName(): string {
    return loadTimeData.getString('hidDetectionContinue');
  }

  canClickNext(): boolean {
    assert(this.nextButton);
    return this.nextButton.isEnabled();
  }
}

class WelcomeScreenTester extends ScreenElementApi {
  private demoModeConfirmationDialog: PolymerElementApi;

  constructor() {
    super('connect');
    this.demoModeConfirmationDialog =
        new PolymerElementApi(this, '#demoModeConfirmationDialog');
  }

  override clickNext(): void {
    if (!this.nextButton) {
      const mainStep = new PolymerElementApi(this, '#welcomeScreen');
      this.nextButton = new PolymerElementApi(mainStep, '#getStarted');
    }

    assert(this.nextButton);
    this.nextButton.click();
  }

  getDemoModeOkButtonName(): string {
    return loadTimeData.getString('enableDemoModeDialogConfirm');
  }
}

class NetworkScreenTester extends ScreenElementApi {
  constructor() {
    super('network-selection');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  override shouldSkip(): boolean {
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
  private personalCrButton: PolymerElementApi;
  private enrollCrButton: PolymerElementApi;
  private enrollTriageCrButton: PolymerElementApi;
  private enrollNextButton: PolymerElementApi;

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
  selectEnrollTriageButton(): void {
    this.enrollTriageCrButton.click();
  }

  /**
   * Presses for personal use button to select it.
   */
  selectPersonalUser(): void {
    this.personalCrButton.click();
  }

  /**
   * Presses for work button to select it.
   */
  selectForWork(): void {
    this.enrollCrButton.click();
  }

  /**
   * Presses next button in enroll-triage step.
   */
  clickEnrollNextButton(): void {
    this.enrollNextButton.click();
  }
}

class GaiaScreenTester extends ScreenElementApi {
  private signinFrame: PolymerElementApi;
  private gaiaDialog: PolymerElementApi;
  private gaiaLoading: PolymerElementApi;

  constructor() {
    super('gaia-signin');
    this.signinFrame = new PolymerElementApi(this, '#signin-frame-dialog');
    this.gaiaDialog = new PolymerElementApi(this.signinFrame, '#gaiaDialog');
    this.gaiaLoading = new PolymerElementApi(this, '#step-loading');
  }

  /**
   * Returns if the Gaia Screen is ready for test interaction.
   */
  isReadyForTesting(): boolean {
    return (
        this.isVisible() && !this.gaiaLoading.isVisible() &&
        this.signinFrame.isVisible() && this.gaiaDialog.isVisible());
  }
}

class SyncScreenTester extends ScreenElementApi {
  private loadedStep: PolymerElementApi;

  constructor() {
    super('sync-consent');
    this.loadedStep = new PolymerElementApi(this, '#syncConsentOverviewDialog');
  }

  /**
   * Returns if the Sync Consent Screen is ready for test interaction.
   */
  isReadyForTesting(): boolean {
    return this.isVisible() && this.loadedStep.isVisible();
  }
}

class PasswordSelectionScreenTester extends ScreenElementApi {
  private localPasswordCrButton: PolymerElementApi;
  private gaiaPasswordCrButton: PolymerElementApi;

  constructor() {
    super('password-selection');
    this.localPasswordCrButton =
        new PolymerElementApi(this, '#localPasswordButton');
    this.gaiaPasswordCrButton =
        new PolymerElementApi(this, '#gaiaPasswordButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  /**
   * Presses Create Chromebook password button to select it.
   */
  selectLocalPassword(): void {
    this.localPasswordCrButton.click();
  }

  /**
   * Presses Use Google Account password button to select it.
   */
  selectGaiaPassword(): void {
    this.gaiaPasswordCrButton.click();
  }
}

class FingerprintScreenTester extends ScreenElementApi {
  constructor() {
    super('fingerprint-setup');
  }
  override shouldSkip(): boolean {
    return !loadTimeData.getBoolean('testapi_isFingerprintSupported');
  }
}

class AiIntroScreenTester extends ScreenElementApi {
  constructor() {
    super('ai-intro');
  }
  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipAiIntro');
  }
}

class GeminiIntroScreenTester extends ScreenElementApi {
  constructor() {
    super('gemini-intro');
  }
  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipGeminiIntro');
  }
}

class AssistantScreenTester extends ScreenElementApi {
  private mainElement: PolymerElementApi;
  private valueProp: PolymerElementApi;
  private valuePropSkipButtonText: PolymerElementApi;
  private relatedInfo: PolymerElementApi;

  constructor() {
    super('assistant-optin-flow');
    this.mainElement = new PolymerElementApi(this, '#card');
    this.valueProp = new PolymerElementApi(this.mainElement, '#valueProp');
    this.valuePropSkipButtonText =
        new PolymerElementApi(this.valueProp, '#skip-button-text');
    this.relatedInfo = new PolymerElementApi(this.mainElement, '#relatedInfo');
  }

  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipAssistant');
  }

  /**
   * Returns if the assistant screen is ready for test interaction.
   */
  isReadyForTesting(): boolean {
    return (
        this.isVisible() &&
        (this.valueProp.isVisible() || this.relatedInfo.isVisible()));
  }

  getSkipButtonName(): string {
    if (this.valueProp.isVisible()) {
      const valuePropSkipButton = this.valuePropSkipButtonText.element();
      if (!valuePropSkipButton || !valuePropSkipButton.textContent) {
        return '';
      }
      return valuePropSkipButton.textContent;
    }
    return loadTimeData.getString('assistantOptinNoThanksButton');
  }

  /**
   * Returns whether we currently show existing user flow.
   */
  isPreviousUserFlowShown(): boolean {
    return this.relatedInfo.isVisible();
  }
}

class MarketingOptInScreenTester extends ScreenElementApi {
  private accessibilityButton: PolymerElementApi;
  private accessibilityStep: PolymerElementApi;
  private accessibilityToggle: PolymerElementApi;
  private marketingOptInGameDeviceTitle: PolymerElementApi;

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

  override shouldSkip(): boolean {
    return !loadTimeData.getBoolean('testapi_isBrandedBuild');
  }

  /**
   * Returns whether accessibility step is shown.
   */
  isAccessibilityStepReadyForTesting(): boolean {
    return this.accessibilityStep.isVisible();
  }

  /**
   * Returns whether a11y button is visible on the marketing-opt-in screen.
   */
  isAccessibilityButtonVisible(): boolean {
    return this.accessibilityButton.isVisible();
  }

  /**
   * Returns whether a11y toggle is on.
   */
  isAccessibilityToggleOn(): boolean {
    const accessibilityToggle = this.accessibilityToggle.element();
    assert(accessibilityToggle instanceof OobeA11yOption);
    return accessibilityToggle.checked;
  }

  /**
   * Returns whether gaming-specific title is visible.
   */
  isMarketingOptInGameDeviceTitleVisible(): boolean {
    return this.marketingOptInGameDeviceTitle.isVisible();
  }

  /**
   * Returns a11y button name.
   */
  getAccessibilityButtonName(): string {
    return loadTimeData.getString('marketingOptInA11yButtonLabel');
  }

  /**
   * Returns name of Done button on a11y page.
   */
  getAccessibilityDoneButtonName(): string {
    return loadTimeData.getString('finalA11yPageDoneButtonTitle');
  }

  /**
   * Returns name of Get Started button.
   */
  getGetStartedButtonName(): string {
    return loadTimeData.getString('marketingOptInScreenAllSet');
  }

  /**
   * Returns gaming-specific title.
   */
  getCloudGamingDeviceTitle(): string {
    return loadTimeData.getString('marketingOptInScreenGameDeviceTitle');
  }
}

class ThemeSelectionScreenTester extends ScreenElementApi {
  private themeRadioButton: PolymerElementApi;
  private lightThemeButton: PolymerElementApi;
  private darkThemeButton: PolymerElementApi;
  private autoThemeButton: PolymerElementApi;
  private textHeader: PolymerElementApi;

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
   */
  isReadyForTesting(): boolean {
    return (
        this.isVisible() && this.lightThemeButton.isVisible() &&
        this.darkThemeButton.isVisible() && this.autoThemeButton.isVisible());
  }

  /**
   * Presses light theme button to select it.
   */
  selectLightTheme(): void {
    this.lightThemeButton.click();
  }

  /**
   * Presses dark theme button to select it.
   */
  selectDarkTheme(): void {
    this.darkThemeButton.click();
  }

  /**
   * Presses auto theme button to select it.
   */
  selectAutoTheme(): void {
    this.autoThemeButton.click();
  }

  /**
   * Finds which theme is selected.
   */
  getNameOfSelectedTheme(): string {
    const themeRadioButton = this.themeRadioButton.element();
    assert(themeRadioButton instanceof CrRadioGroupElement);
    return themeRadioButton.selected;
  }

  /**
   * Retrieves computed color of the screen header. This value will be used to
   * determine screen's color mode.
   */
  getHeaderTextColor(): string {
    const el = this.textHeader.element();
    assert(el instanceof HTMLElement);
    return window.getComputedStyle(el).color;
  }
}

class ConfirmSamlPasswordScreenTester extends ScreenElementApi {
  private passwordInput: TextFieldApi;
  private confirmPasswordInput: TextFieldApi;

  constructor() {
    super('saml-confirm-password');
    this.passwordInput = new TextFieldApi(this, '#passwordInput');
    this.confirmPasswordInput = new TextFieldApi(this, '#confirmPasswordInput');
    this.nextButton = new PolymerElementApi(this, '#next');
  }

  /**
   * Enter password input fields with password value and submit the form.
   */
  enterManualPasswords(password: string) {
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
  private skipButton: PolymerElementApi;
  private doneButton: PolymerElementApi;
  private backButton: PolymerElementApi;
  private pinField: TextFieldApi;
  private pinButtons: Record<string, PolymerElementApi>;

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
   */
  enterPin(pin: string): void {
    this.pinField.typeInto(pin);
  }

  /**
   * Presses a single digit button in the PIN keyboard.
   * @param digit String with single digit to be clicked on.
   */
  pressPinDigit(digit: string): void {
    this.pinButtons[digit].click();
  }

  /**
   */
  getSkipButtonName(): string {
    return loadTimeData.getString('discoverPinSetupSkip');
  }

  isInTabletMode(): boolean {
    return loadTimeData.getBoolean('testapi_isOobeInTabletMode');
  }
}

class EnrollmentSignInStep extends PolymerElementApi {
  private signInFrame: PolymerElementApi;
  private nextButton: PolymerElementApi;
  private backButton: PolymerElementApi;

  constructor(parent: ScreenElementApi) {
    super(parent, '#step-signin');
    this.signInFrame = new PolymerElementApi(this, '#signin-frame');
    this.nextButton = new PolymerElementApi(this, '#primary-action-button');
    this.backButton = new PolymerElementApi(this, '#signin-back-button');
  }

  /**
   * Returns if the Enrollment Signing step is ready for test interaction.
   */
  isReadyForTesting(): boolean {
    return (
        this.isVisible() && this.signInFrame.isVisible() &&
        this.nextButton.isVisible());
  }
}

class EnrollmentAttributeStep extends PolymerElementApi {
  private skipButton: PolymerElementApi;

  constructor(parent: ScreenElementApi) {
    super(parent, '#step-attribute-prompt');
    this.skipButton = new PolymerElementApi(parent, '#attributesSkip');
  }

  isReadyForTesting(): boolean {
    return this.isVisible() && this.skipButton.isVisible();
  }

  clickSkip(): void {
    this.skipButton.click();
  }
}

class EnrollmentSuccessStep extends PolymerElementApi {
  private nextButton: PolymerElementApi;

  constructor(parent: ScreenElementApi) {
    super(parent, '#step-success');
    this.nextButton = new PolymerElementApi(parent, '#successDoneButton');
  }

  /**
   * Returns if the Enrollment Success step is ready for test interaction.
   */
  isReadyForTesting(): boolean {
    return this.isVisible() && this.nextButton.isVisible();
  }

  clickNext(): void {
    this.nextButton.click();
  }
}

class EnrollmentErrorStep extends PolymerElementApi {
  private retryButton: PolymerElementApi;
  private errorMsgContainer: PolymerElementApi;

  constructor(parent: ScreenElementApi) {
    super(parent, '#step-error');
    this.retryButton = new PolymerElementApi(parent, '#errorRetryButton');
    this.errorMsgContainer = new PolymerElementApi(parent, '#errorMsg');
  }

  /**
   * Returns if the Enrollment Error step is ready for test interaction.
   */
  isReadyForTesting(): boolean {
    return this.isVisible() && this.errorMsgContainer.isVisible();
  }

  /**
   * Returns if enterprise enrollment can be retried.
   */
  canRetryEnrollment(): boolean {
    return this.retryButton.isVisible() && this.retryButton.isEnabled();
  }

  /**
   * Click the Retry button on the enrollment error screen.
   */
  clickRetryButton(): void {
    assert(this.canRetryEnrollment());
    this.retryButton.click();
  }

  /**
   * Returns the error text shown on the enrollment error screen.
   */
  getErrorMsg(): string {
    assert(this.isReadyForTesting());
    const el = this.errorMsgContainer.element();
    assert(el instanceof HTMLElement);
    return el.innerText;
  }
}

class EnterpriseEnrollmentScreenTester extends ScreenElementApi {
  private signInStep: EnrollmentSignInStep;
  private attributeStep: EnrollmentAttributeStep;
  private successStep: EnrollmentSuccessStep;
  private errorStep: EnrollmentErrorStep;
  private enrollmentInProgressDlg: PolymerElementApi;

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
   */
  isEnrollmentInProgress(): boolean {
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
   */
  isReadyForTesting(): boolean {
    return this.isVisible() && this.nextButton !== undefined &&
        this.nextButton.isVisible();
  }

  /**
   * Returns the email field name on the Offline Login Screen.
   */
  getEmailFieldName(): string {
    return loadTimeData.getString('offlineLoginEmail');
  }

  /**
   * Returns the password field name on the Offline Login Screen.
   */
  getPasswordFieldName(): string {
    return loadTimeData.getString('offlineLoginPassword');
  }

  /**
   * Returns the next button name on the Offline Login Screen.
   */
  getNextButtonName(): string {
    return loadTimeData.getString('offlineLoginNextBtn');
  }
}

class ErrorScreenTester extends ScreenElementApi {
  private offlineLink: PolymerElementApi;
  private errorTitle: PolymerElementApi;
  private errorSubtitle: PolymerElementApi;

  constructor() {
    super('error-message');
    this.offlineLink = new PolymerElementApi(this, '#error-offline-login-link');
    this.errorTitle = new PolymerElementApi(this, '#error-title');
    this.errorSubtitle = new PolymerElementApi(this, '#error-subtitle');
  }

  /**
   * Returns if the Error Screen is ready for test interaction.
   */
  isReadyForTesting(): boolean {
    return this.isVisible();
  }

  /**
   *
   * Returns if offline link is visible.
   */
  isOfflineLinkVisible(): boolean {
    return this.offlineLink.isVisible();
  }

  /**
   * Returns error screen message title.
   */
  getErrorTitle(): string {
    // If screen is not visible always return empty title.
    if (!this.isVisible()) {
      return '';
    }
    const el = this.errorTitle.element();
    assert(el instanceof HTMLElement);
    return el.innerText.trim();
  }

  /**
   * Returns error screen subtitle. Includes all visible error messages.
   */
  getErrorReasons(): string {
    // If screen is not visible always return empty reasons.
    if (!this.isVisible()) {
      return '';
    }
    const el = this.errorSubtitle.element();
    assert(el instanceof HTMLElement);
    return el.innerText.trim();
  }

  /**
   * Click the sign in as an existing user Link.
   */
  clickSignInAsExistingUserLink(): void {
    this.offlineLink.click();
  }
}

class DemoPreferencesScreenTester extends ScreenElementApi {
  constructor() {
    super('demo-preferences');
  }

  getDemoPreferencesNextButtonName(): string {
    return loadTimeData.getString('demoPreferencesNextButtonLabel');
  }
}

class GuestTosScreenTester extends ScreenElementApi {
  private loadedStep: PolymerElementApi;
  private googleEulaDialog: PolymerElementApi;
  private crosEulaDialog: PolymerElementApi;
  private googleEulaDialogLink: PolymerElementApi;
  private crosEulaDialogLink: PolymerElementApi;

  constructor() {
    super('guest-tos');
    this.loadedStep = new PolymerElementApi(this, '#loaded');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');

    this.googleEulaDialog = new PolymerElementApi(this, '#googleEulaDialog');
    this.crosEulaDialog = new PolymerElementApi(this, '#crosEulaDialog');

    this.googleEulaDialogLink = new PolymerElementApi(this, '#googleEulaLink');
    this.crosEulaDialogLink = new PolymerElementApi(this, '#crosEulaLink');
  }

  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipGuestTos');
  }

  isReadyForTesting(): boolean {
    return this.isVisible() && this.loadedStep.isVisible();
  }

  getNextButtonName(): string {
    return loadTimeData.getString('guestTosAccept');
  }

  getEulaButtonName(): string {
    return loadTimeData.getString('guestTosOk');
  }

  isGoogleEulaDialogShown(): boolean {
    return this.googleEulaDialog.isVisible();
  }

  isCrosEulaDialogShown(): boolean {
    return this.crosEulaDialog.isVisible();
  }

  getGoogleEulaLinkName(): string {
    const googleEulaLink = this.googleEulaDialogLink.element();
    assert(googleEulaLink instanceof HTMLAnchorElement);
    return googleEulaLink.text.trim();
  }

  getCrosEulaLinkName(): string {
    const crosEulaLink = this.crosEulaDialogLink.element();
    assert(crosEulaLink instanceof HTMLAnchorElement);
    return crosEulaLink.text.trim();
  }
}

class GestureNavigationScreenTester extends ScreenElementApi {
  constructor() {
    super('gesture-navigation');
  }

  getNextButtonName(): string {
    return loadTimeData.getString('gestureNavigationIntroNextButton');
  }

  getSkipButtonName(): string {
    return loadTimeData.getString('gestureNavigationIntroSkipButton');
  }
}

class ConsolidatedConsentScreenTester extends ScreenElementApi {
  private loadedStep: PolymerElementApi;
  private readMoreButton: PolymerElementApi;
  private recoveryToggle: PolymerElementApi;
  private googleEulaDialog: PolymerElementApi;
  private crosEulaDialog: PolymerElementApi;
  private arcTosDialog: PolymerElementApi;
  private privacyPolicyDialog: PolymerElementApi;
  private googleEulaLink: PolymerElementApi;
  private crosEulaLink: PolymerElementApi;
  private arcTosLink: PolymerElementApi;
  private privacyPolicyLink: PolymerElementApi;

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

  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipConsolidatedConsent');
  }

  isReadyForTesting(): boolean {
    return this.isVisible() && this.loadedStep.isVisible();
  }

  isReadMoreButtonShown(): boolean {
    // The read more button is inside a <dom-if> element, if it's hidden, the
    // element would be removed entirely from dom, so we need to check if the
    // element exists before checking if it's visible.
    return (
        this.readMoreButton.element() !== null &&
        this.readMoreButton.isVisible());
  }

  getNextButtonName(): string {
    return loadTimeData.getString('consolidatedConsentAcceptAndContinue');
  }

  /**
   * Enable the toggle which controls whether the user opted-in the the
   * cryptohome recovery feature.
   */
  enableRecoveryToggle(): void {
    const recoveryToggle = this.recoveryToggle.element();
    assert(recoveryToggle instanceof CrToggleElement);
    recoveryToggle.checked = true;
  }

  getEulaOkButtonName(): string {
    return loadTimeData.getString('consolidatedConsentOK');
  }

  isGoogleEulaDialogShown(): boolean {
    return this.googleEulaDialog.isVisible();
  }

  isCrosEulaDialogShown(): boolean {
    return this.crosEulaDialog.isVisible();
  }

  isArcTosDialogShown(): boolean {
    return this.arcTosDialog.isVisible();
  }

  isPrivacyPolicyDialogShown(): boolean {
    return this.privacyPolicyDialog.isVisible();
  }

  getGoogleEulaLinkName(): string {
    const googleEulaLink = this.googleEulaLink.element();
    assert(googleEulaLink instanceof HTMLAnchorElement);
    return googleEulaLink.text.trim();
  }

  getCrosEulaLinkName(): string {
    const crosEulaLink = this.crosEulaLink.element();
    assert(crosEulaLink instanceof HTMLAnchorElement);
    return crosEulaLink.text.trim();
  }

  getArcTosLinkName(): string {
    const arcTosLink = this.arcTosLink.element();
    assert(arcTosLink instanceof HTMLAnchorElement);
    return arcTosLink.text.trim();
  }

  getPrivacyPolicyLinkName(): string {
    const privacyPolicyLink = this.privacyPolicyLink.element();
    assert(privacyPolicyLink instanceof HTMLAnchorElement);
    return privacyPolicyLink.text.trim();
  }

  /**
   * Click `accept` button to go to the next screen.
   */
  clickAcceptButton(): void {
    assert(this.nextButton);
    const el = this.nextButton.element();
    assert(el instanceof HTMLElement);
    el.click();
  }
}

class SmartPrivacyProtectionScreenTester extends ScreenElementApi {
  private noThanks: PolymerElementApi;
  constructor() {
    super('smart-privacy-protection');
    this.noThanks = new PolymerElementApi(this, '#noThanksButton');
  }

  override shouldSkip(): boolean {
    return !loadTimeData.getBoolean('testapi_isHPSEnabled');
  }

  isReadyForTesting(): boolean {
    return this.isVisible() && this.noThanks.isVisible();
  }

  getNoThanksButtonName(): string {
    return loadTimeData.getString('smartPrivacyProtectionTurnOffButton');
  }
}

class CryptohomeRecoverySetupScreenTester extends ScreenElementApi {
  constructor() {
    super('cryptohome-recovery-setup');
  }
}

class LocalPasswordSetupScreenTester extends ScreenElementApi {
  private passwordInput: PolymerElementApi;
  private firstInput: TextFieldApi;
  private confirmInput: TextFieldApi;

  constructor() {
    super('local-password-setup');
    this.passwordInput = new PolymerElementApi(this, '#passwordInput');
    this.firstInput = new TextFieldApi(this.passwordInput, '#firstInput');
    this.confirmInput = new TextFieldApi(this.passwordInput, '#confirmInput');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  isReadyForTesting(): boolean {
    return this.isVisible() && this.firstInput.isVisible() &&
        this.confirmInput.isVisible();
  }

  // TODO (b/329361749): remove this code after crrev.com/c/5381081 landed.
  enterPassword(password: string): void {
    this.firstInput.typeInto(password);
    afterNextRender(assert(this.element()), () => {
      this.confirmInput.typeInto(password);
      afterNextRender(assert(this.element()), () => {
        assert(this.nextButton);
        this.nextButton.click();
      });
    });
  }

  enterPasswordToFirstInput(password: string): void {
    this.firstInput.typeInto(password);
  }

  enterPasswordToConfirmInput(password: string): void {
    this.confirmInput.typeInto(password);
  }

  isNextButtonEnabled(): boolean {
    return this.nextButton !== undefined && this.nextButton.isEnabled();
  }
}

class PasswordFactorSuccessScreenTester extends ScreenElementApi {
  private doneButton: PolymerElementApi;

  constructor() {
    super('factor-setup-success');
    this.doneButton = new PolymerElementApi(this, '#doneButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  isDone(): boolean {
    assert(this.nextButton);
    return this.isVisible() &&
        (this.doneButton.isVisible() || this.nextButton.isVisible());
  }

  clickDone(): void {
    if (this.doneButton.isVisible()) {
      this.doneButton.click();
      return;
    }
    if (this.nextButton && this.nextButton.isVisible()) {
      this.nextButton.click();
    }
  }
}

class GaiaInfoScreenTester extends ScreenElementApi {
  private manualCredentialsButton: PolymerElementApi;

  constructor() {
    super('gaia-info');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.manualCredentialsButton = new PolymerElementApi(this, '#manualButton');
  }

  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipGaiaInfoScreen');
  }

  isOobeQuickStartEnabled(): boolean {
    return loadTimeData.getBoolean('testapi_isOobeQuickStartEnabled');
  }

  /**
   * Select option to manually enter Google credentials.
   */
  selectManualCredentials(): void {
    this.manualCredentialsButton.click();
  }
}

class ConsumerUpdateScreenTester extends ScreenElementApi {
  private skipButton: PolymerElementApi;

  constructor() {
    super('consumer-update');
    this.skipButton = new PolymerElementApi(this, '#skipButton');
  }

  clickSkip(): void {
    this.skipButton.click();
  }
}

class ChoobeScreenTester extends ScreenElementApi {
  private skipButton: PolymerElementApi;
  private choobeScreensList: PolymerElementApi;
  private touchpadScrollScreenButton: PolymerElementApi;
  private drivePinningScreenButton: PolymerElementApi;
  private displaySizeScreenButton: PolymerElementApi;
  private themeSelectionScreenButton: PolymerElementApi;
  private shouldSkipReceived: boolean;
  private shouldBeSkipped: boolean;

  constructor() {
    super('choobe');
    this.skipButton = new PolymerElementApi(this, '#skipButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.choobeScreensList = new PolymerElementApi(this, '#screensList');
    this.touchpadScrollScreenButton = new PolymerElementApi(
        this.choobeScreensList, '#cr-button-touchpad-scroll');
    this.drivePinningScreenButton = new PolymerElementApi(
        this.choobeScreensList, '#cr-button-drive-pinning');
    this.displaySizeScreenButton = new PolymerElementApi(
        this.choobeScreensList, '#cr-button-display-size');
    this.themeSelectionScreenButton = new PolymerElementApi(
        this.choobeScreensList, '#cr-button-theme-selection');
    this.shouldSkipReceived = false;
    this.shouldBeSkipped = false;
  }

  requestShouldSkip(): void {
    sendWithPromise('OobeTestApi.getShouldSkipChoobe')
        .then(shouldBeSkipped => this.setShouldBeSkipped(shouldBeSkipped));
  }

  setShouldBeSkipped(shouldBeSkipped: boolean): void {
    this.shouldSkipReceived = true;
    this.shouldBeSkipped = shouldBeSkipped;
  }

  isShouldSkipReceived(): boolean {
    return this.shouldSkipReceived;
  }

  override shouldSkip(): boolean {
    assert(
        this.isShouldSkipReceived(),
        '`shouldSkip()` should only be called after `requestShouldSkip()`' +
            'is called, and `isShouldSkippedReceived()` starts returning true');
    return this.shouldBeSkipped;
  }

  // TODO(b/327270907): Remove `updatedShouldSkip()` after the users of the test
  // API migrate to using `shouldSkip()`
  updatedShouldSkip(): boolean {
    return this.shouldSkip();
  }

  isReadyForTesting(): boolean {
    return this.isVisible();
  }

  clickTouchpadScrollScreen(): void {
    this.touchpadScrollScreenButton.click();
  }

  clickDrivePinningScreen(): void {
    this.drivePinningScreenButton.click();
  }

  clickDisplaySizeScreen(): void {
    this.displaySizeScreenButton.click();
  }

  clickThemeSelectionScreen(): void {
    this.themeSelectionScreenButton.click();
  }

  isTouchpadScrollScreenVisible(): boolean {
    return this.touchpadScrollScreenButton.element() !== null &&
        this.touchpadScrollScreenButton.isVisible();
  }

  isDrivePinningScreenVisible(): boolean {
    return this.drivePinningScreenButton.element() !== null &&
        this.drivePinningScreenButton.isVisible();
  }

  isDisplaySizeScreenVisible(): boolean {
    return this.displaySizeScreenButton.element() !== null &&
        this.displaySizeScreenButton.isVisible();
  }

  isThemeSelectionScreenVisible(): boolean {
    return this.themeSelectionScreenButton.element() !== null &&
        this.themeSelectionScreenButton.isVisible();
  }

  isDrivePinningScreenChecked(): boolean {
    const el = this.drivePinningScreenButton.element();
    assert(el instanceof HTMLElement);
    return !!el.getAttribute('checked');
  }

  clickSkip(): void {
    this.skipButton.click();
  }
}

class ChoobeDrivePinningScreenTester extends ScreenElementApi {
  private drivePinningToggle: PolymerElementApi;
  private drivePinningSpaceInformation: PolymerElementApi;

  constructor() {
    super('drive-pinning');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.drivePinningToggle =
        new PolymerElementApi(this, '#drivePinningToggle');
    this.drivePinningSpaceInformation =
        new PolymerElementApi(this, '#spaceInformation');
  }

  isReadyForTesting(): boolean {
    return this.isVisible() && this.drivePinningToggle.isVisible() &&
        this.drivePinningSpaceInformation.isVisible();
  }

  toggleFileSync(): void {
    this.drivePinningToggle.click();
  }

  isFileSyncEnabled(): boolean {
    const drivePinningToggle = this.drivePinningToggle.element();
    assert(drivePinningToggle instanceof CrToggleElement);
    return !!drivePinningToggle.checked;
  }

  getSpaceInformationString(): string {
    const el = this.drivePinningSpaceInformation.element();
    assert(el instanceof HTMLElement);
    return el.innerText;
  }
}


class ChoobeTouchpadScrollScreenTester extends ScreenElementApi {
  private shouldSkipReceived: boolean;
  private shouldBeSkipped: boolean;

  constructor() {
    super('touchpad-scroll');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
    this.shouldSkipReceived = false;
    this.shouldBeSkipped = false;
  }

  requestShouldSkip(): void {
    sendWithPromise('OobeTestApi.getShouldSkipTouchpadScroll')
        .then(shouldBeSkipped => this.setShouldBeSkipped(shouldBeSkipped));
  }

  setShouldBeSkipped(shouldBeSkipped: boolean): void {
    this.shouldSkipReceived = true;
    this.shouldBeSkipped = shouldBeSkipped;
  }

  isShouldSkipReceived(): boolean {
    return this.shouldSkipReceived;
  }

  override shouldSkip(): boolean {
    assert(
        this.isShouldSkipReceived(),
        '`shouldSkip()` should only be called after `requestShouldSkip()`' +
            'is called, and `isShouldSkippedReceived()` starts returning true');
    return this.shouldBeSkipped;
  }

  // TODO(b/327270907): Remove `updatedShouldSkip()` after the users of the test
  // API migrate to using `shouldSkip()`
  updatedShouldSkip(): boolean {
    return this.shouldSkip();
  }

  isReadyForTesting(): boolean {
    return this.isVisible();
  }
}

class ChoobeDisplaySizeTester extends ScreenElementApi {
  constructor() {
    super('display-size');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipDisplaySize');
  }

  isReadyForTesting(): boolean {
    return this.isVisible();
  }
}

class HwDataCollectionScreenTester extends ScreenElementApi {
  constructor() {
    super('hw-data-collection');
    this.nextButton = new PolymerElementApi(this, '#acceptButton');
  }

  override shouldSkip(): boolean {
    return loadTimeData.getBoolean('testapi_shouldSkipHwDataCollection');
  }

  isReadyForTesting(): boolean {
    return this.isVisible();
  }
}

class DeviceUseCaseScreenTester extends ScreenElementApi {
  private loadingStep: PolymerElementApi;
  private overviewStep: PolymerElementApi;
  private skipButton: PolymerElementApi;

  constructor() {
    super('categories-selection');
    this.loadingStep = new PolymerElementApi(this, '#progressDialog');
    this.overviewStep = new PolymerElementApi(this, '#categoriesDialog');
    this.skipButton = new PolymerElementApi(this, '#skipButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  isReadyForTesting(): boolean {
    // Return true only if we were able to fetch data from the server and
    // rendered it on the screen.
    return this.isVisible() && this.overviewStep.isVisible();
  }
}

class PersonalizedRecommendAppsScreenTester extends ScreenElementApi {
  private loadingStep: PolymerElementApi;
  private overviewStep: PolymerElementApi;
  private skipButton: PolymerElementApi;

  constructor() {
    super('personalized-apps');
    this.loadingStep = new PolymerElementApi(this, '#progressDialog');
    this.overviewStep =
        new PolymerElementApi(this, '#personalizedRecommendDialog');
    this.skipButton = new PolymerElementApi(this, '#skipButton');
    this.nextButton = new PolymerElementApi(this, '#nextButton');
  }

  isReadyForTesting(): boolean {
    // Return true only if we were able to fetch data from the server and
    // rendered it on the screen.
    return this.isVisible() && this.overviewStep.isVisible();
  }
}

class SplitModifierKeyboardInfoScreenTester extends ScreenElementApi {
  constructor() {
    super('split-modifier-keyboard-info');
  }

  override shouldSkip(): boolean {
    return loadTimeData.getBoolean(
        'testapi_shouldSkipSplitModifierKeyboardInfo');
  }

  isReadyForTesting(): boolean {
    return this.isVisible();
  }
}

export class OobeApiProvider {
  private screens: Record<string, ScreenElementApi>;
  private metricsClientID: string;

  private loginWithPin: (username: string, pin: string) => void;
  private advanceToScreen: (screen: string) => void;
  private skipToLoginForTesting: () => void;
  private skipPostLoginScreens: () => void;
  private loginAsGuest: () => void;
  private showGaiaDialog: () => void;
  private getBrowseAsGuestButtonName: () => void;
  private getCurrentScreenName: () => string;
  private getCurrentScreenStep: () => string;
  private getOobeActiveDialog: () => HTMLElement | null;
  private findActiveOobeDialogSlotsByName: (slotName: string) => HTMLElement[];
  private combineTextOfAdaptiveDialogSlots: (slotName: string) => string;
  private getOobeActiveDialogTitleText: () => string;
  private getOobeActiveDialogSubtitleText: () => string;
  private getOobeActiveDialogContentText: () => string;

  private requestMetricsClientID: () => void;
  private isMetricsClientIdAvailable: () => boolean;
  private getMetricsClientID: () => string;

  constructor() {
    this.screens = {
      HIDDetectionScreen: new HidDetectionScreenTester(),
      WelcomeScreen: new WelcomeScreenTester(),
      NetworkScreen: new NetworkScreenTester(),
      UpdateScreen: new UpdateScreenTester(),
      EnrollmentScreen: new EnrollmentScreenTester(),
      UserCreationScreen: new UserCreationScreenTester(),
      GaiaScreen: new GaiaScreenTester(),
      SyncScreen: new SyncScreenTester(),
      PasswordSelectionScreen: new PasswordSelectionScreenTester(),
      FingerprintScreen: new FingerprintScreenTester(),
      AiIntroScreen: new AiIntroScreenTester(),
      GeminiIntroScreen: new GeminiIntroScreenTester(),
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
      PasswordFactorSuccessScreen: new PasswordFactorSuccessScreenTester(),
      GaiaInfoScreen: new GaiaInfoScreenTester(),
      ConsumerUpdateScreen: new ConsumerUpdateScreenTester(),
      ChoobeScreen: new ChoobeScreenTester(),
      ChoobeDrivePinningScreen: new ChoobeDrivePinningScreenTester(),
      ChoobeTouchpadScrollScreen: new ChoobeTouchpadScrollScreenTester(),
      ChoobeDisplaySizeScreen: new ChoobeDisplaySizeTester(),
      HWDataCollectionScreen: new HwDataCollectionScreenTester(),
      DeviceUseCaseScreen: new DeviceUseCaseScreenTester(),
      PersonalizedRecommendAppsScreen:
          new PersonalizedRecommendAppsScreenTester(),
      SplitModifierKeyboardInfoScreen:
          new SplitModifierKeyboardInfoScreenTester(),
    };

    this.loginWithPin = function(username: string, pin: string): void {
      chrome.send('OobeTestApi.loginWithPin', [username, pin]);
    };

    this.advanceToScreen = function(screen: string): void {
      chrome.send('OobeTestApi.advanceToScreen', [screen]);
    };

    this.skipToLoginForTesting = function(): void {
      chrome.send('OobeTestApi.skipToLoginForTesting');
    };

    this.skipPostLoginScreens = function(): void {
      chrome.send('OobeTestApi.skipPostLoginScreens');
    };

    this.loginAsGuest = function(): void {
      chrome.send('OobeTestApi.loginAsGuest');
    };

    this.showGaiaDialog = function(): void {
      chrome.send('OobeTestApi.showGaiaDialog');
    };

    this.getBrowseAsGuestButtonName = function(): string {
      return loadTimeData.getString('testapi_browseAsGuest');
    };

    this.getCurrentScreenName = function(): string {
      const currentScreen = Oobe.getInstance().currentScreen;
      return currentScreen ? currentScreen.id.trim() : 'none';
    };

    this.getCurrentScreenStep = function(): string {
      const currentScreen = Oobe.getInstance().currentScreen;
      if (currentScreen === null) {
        return 'none';
      }
      const step = currentScreen.getAttribute('multistep');
      if (step === null) {
        return 'default';
      }
      return step.trim();
    };

    /**
     * Returns currently displayed OOBE dialog HTML element.
     */
    this.getOobeActiveDialog = function(): HTMLElement|null {
      const adaptiveDialogs =
          Oobe.getInstance().currentScreen?.shadowRoot?.querySelectorAll(
              'oobe-adaptive-dialog');
      if (adaptiveDialogs) {
        for (const dialog of adaptiveDialogs) {
          // Only one adaptive dialog could be shown at the same time.
          if (!dialog.hidden) {
            return dialog;
          }
        }
      }

      // If we didn't find any active adaptive dialog we might currently display
      // a loading dialog, a special wrapper over oobe-adaptive-dialog to show
      // loading process.
      // In this case we can try to fetch all loading dialogs attached to the
      // current screen and check their internal adaptive dialogs.
      const loadingDialogs =
          Oobe.getInstance().currentScreen?.shadowRoot?.querySelectorAll(
              'oobe-loading-dialog');
      if (loadingDialogs) {
        for (const dialog of loadingDialogs) {
          if (!dialog.hidden) {
            const activeDialog =
                dialog.shadowRoot?.querySelector('oobe-adaptive-dialog');
            return activeDialog ? activeDialog : null;
          }
        }
      }

      return null;
    };

    /**
     * Returns array of the slot HTML elements with a given slot name.
     */
    this.findActiveOobeDialogSlotsByName = function(slotName: string):
        HTMLElement[] {
          const dialog = this.getOobeActiveDialog();
          if (dialog === null) {
            return [];
          }

          if (dialog.children === undefined || dialog.children === null) {
            return [];
          }

          // There are cases when we have different element with the same slot,
          // so we must find all of them for a given adaptive dialog.
          const result = [];

          for (const child of dialog.children) {
            if (!(child instanceof HTMLElement)) {
              continue;
            }
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
     */
    this.combineTextOfAdaptiveDialogSlots = function(slotName: string): string {
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
     */
    this.getOobeActiveDialogTitleText = function(): string {
      return this.combineTextOfAdaptiveDialogSlots('title');
    };

    /**
     * Returns text inside displayed subtitle slots.
     */
    this.getOobeActiveDialogSubtitleText = function(): string {
      return this.combineTextOfAdaptiveDialogSlots('subtitle');
    };

    /**
     * Returns text inside displayed content slots.
     */
    this.getOobeActiveDialogContentText = function(): string {
      return this.combineTextOfAdaptiveDialogSlots('content');
    };

    this.isMetricsClientIdAvailable = function(): boolean {
      return this.metricsClientID !== '';
    };

    this.requestMetricsClientID = function(): void {
      sendWithPromise('OobeTestApi.getMetricsClientID')
          .then(clientID => this.onMetricsClientIdReceived(clientID));
    };

    this.getMetricsClientID = function(): string {
      assert(
          this.isMetricsClientIdAvailable(),
          '`getMetricsClientID()` should only be called after ' +
              '`requestMetricsClientID()` is called, and ' +
              '`isMetricsClientIdAvailable()` starts returning true');

      const id = this.metricsClientID;

      // Reset `this.metricsClientID` when it is consumed to force
      // the next caller to call first this.requestMetricsClientID().
      this.metricsClientID = '';

      return id;
    };
  }

  onMetricsClientIdReceived(clientID: string): void {
    this.metricsClientID = clientID;
  }
}
