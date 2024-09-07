// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/security_token_pin.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/dialogs/oobe_modal_dialog.js';
import '../../components/gaia_dialog.js';

import {Authenticator, AuthFlow, AuthMode, SUPPORTED_PARAMS} from '//oobe/gaia_auth_host/authenticator.js';
import {assert} from '//resources/js/assert.js';
import {sendWithPromise} from '//resources/js/cr.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import type {GaiaDialog} from '../../components/gaia_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeTypes} from '../../components/oobe_types.js';
import type {SecurityTokenPin} from '../../components/security_token_pin.js';
import {Oobe} from '../../cr_ui.js';

import {getTemplate} from './gaia_signin.html.js';

// GAIA animation guard timer. Started when GAIA page is loaded (Authenticator
// 'ready' event) and is intended to guard against edge cases when 'showView'
// message is not generated/received.
const GAIA_ANIMATION_GUARD_MILLISEC = 300;

// Maximum Gaia loading time in seconds.
const MAX_GAIA_LOADING_TIME_SEC = 60;

// Amount of time allowed for video based SAML logins, to prevent a site from
// keeping the camera on indefinitely.  This is a hard deadline and it will
// not be extended by user activity.
const VIDEO_LOGIN_TIMEOUT = 180 * 1000;

/**
 * The authentication mode for the screen.
 */
enum ScreenAuthMode {
  DEFAULT = 0,        // Default GAIA login flow.
  SAML_REDIRECT = 1,  // SAML redirection.
}

/**
 * UI mode for the dialog.
 */
enum DialogMode {
  GAIA = 'online-gaia',
  LOADING = 'loading',
  PIN_DIALOG = 'pin',
}

/**
 * Steps that could be the first one in the flow.
 */
const POSSIBLE_FIRST_SIGNIN_STEPS = [DialogMode.GAIA, DialogMode.LOADING];

/**
 * This enum is tied directly to the `EnrollmentNudgeUserAction` UMA enum
 * defined in //tools/metrics/histograms/enums.xml, and to the
 * `EnrollmentNudgeTest::UserAction` enum defined in
 * //chrome/browser/ash/login/enrollment_nudge_browsertest.cc.
 * Do not change one without changing the others.
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 */
enum EnrollmentNudgeUserAction {
  ENTERPRISE_ENROLLMENT_BUTTON = 0,
  USE_ANOTHER_ACCOUNT_BUTTON = 1,
  MAX = 2,
}

const GaiaSigninElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

interface GaiaSigninScreenData {
  hasUserPods: boolean;
}

export class GaiaSigninElement extends GaiaSigninElementBase {
  static get is() {
    return 'gaia-signin-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether the screen contents are currently being loaded.
       */
      loadingFrameContents: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the loading UI is shown.
       */
      isLoadingUiShown: {
        type: Boolean,
        computed: 'computeIsLoadingUiShown(loadingFrameContents, ' +
            'authCompleted)',
      },

      /**
       * Whether the navigation controls are enabled.
       */
      navigationEnabled: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether the authenticator is currently redirected to |SAML| flow. It is
       * set to true early during a default redirection to address situations
       * when an error during loading the 3P IdP occurs and no change in
       * `authFlow` happens, but the UI for 3P IdP still should be shown.
       * Updated on `authFlow` change.
       */
      isSaml: {
        type: Boolean,
        value: false,
        observer: 'onSamlChanged',
      },

      /**
       * Contains the security token PIN dialog parameters object when the
       * dialog is shown. Is null when no PIN dialog is shown.
       */
      pinDialogParameters: {
        type: Object,
        value: null,
        observer: 'onPinDialogParametersChanged',
      },

      /**
       * Whether the SAML 3rd-party page is visible.
       */
      isSamlSsoVisible: {
        type: Boolean,
        computed: 'computeSamlSsoVisible(isSaml, pinDialogParameters)',
      },

      /**
       * Bound to gaia-dialog::videoEnabled.
       */
      videoEnabled: {
        type: Boolean,
        observer: 'onVideoEnabledChange',
      },

      /**
       * Bound to gaia-dialog::authDomain.
       */
      authDomain: {
        type: String,
      },

      /**
       * Bound to gaia-dialog::authFlow.
       */
      authFlow: {
        type: Number,
        observer: 'onAuthFlowChange',
      },

      /**
       */
      navigationButtonsHidden: {
        type: Boolean,
        value: false,
      },

      /**
       * Bound to gaia-dialog::canGoBack.
       */
      canGaiaGoBack: {
        type: Boolean,
      },

      /*
       * Updates whether the Guest and Apps button is allowed to be shown.
       * (Note that the C++ side contains additional logic that decides whether
       * the Guest button should be shown.)
       */
      firstSigninStep: {
        type: Boolean,
        computed: 'isFirstSigninStep(uiStep, canGaiaGoBack, isSaml)',
        observer: 'onIsFirstSigninStepChanged',
      },

      /*
       * Whether the screen is shown.
       */
      isShown: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the default SAML 3rd-party page is visible. We need to track
       * this in addition to `isSamlSsoVisible` because it has some UI
       * implications (e.g. user needs to be able to switch to non-default IdP).
       */
      isDefaultSsoProvider: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the screen can be hidden.
       */
      isClosable: {
        type: Boolean,
        value: false,
      },

      /**
       * Domain extracted from user's email.
       */
      emailDomain: {
        type: String,
      },
    };
  }

  private loadingFrameContents: boolean;
  private isLoadingUiShown: boolean;
  private navigationEnabled: boolean;
  private isSaml: boolean;
  private pinDialogParameters: OobeTypes.SecurityTokenPinDialogParameters|null;
  private isSamlSsoVisible: boolean;
  private videoEnabled: boolean;
  private authDomain: string;
  private authFlow: number;
  private navigationButtonsHidden: boolean;
  private canGaiaGoBack: boolean;
  private firstSigninStep: boolean;
  private isShown: boolean;
  private isDefaultSsoProvider: boolean;
  private isClosable: boolean;
  private emailDomain: string;
  private authenticatorParams: null|any;
  private email: string;
  private loadingTimer: number|undefined;
  private loadAnimationGuardTimer: number|undefined;
  private videoTimer: number|undefined;
  private showViewProcessed: boolean;
  private authCompleted: boolean;
  private pinDialogResultReported: boolean;

  constructor() {
    super();
    /**
     * Saved authenticator load params.
     */
    this.authenticatorParams = null;

    /**
     * Email of the user, which is logging in using offline mode.
     */
    this.email = '';

    /**
     * Timer id of pending load.
     */
    this.loadingTimer = undefined;

    /**
     * Timer id of a guard timer that is fired in case 'showView' message is not
     * received from GAIA.
     */
    this.loadAnimationGuardTimer = undefined;

    /**
     * Timer id of the video login timer.
     */
    this.videoTimer = undefined;

    /**
     * Whether we've processed 'showView' message - either from GAIA or from
     * guard timer.
     */
    this.showViewProcessed = false;

    /**
     * Whether we've processed 'authCompleted' message.
     */
    this.authCompleted = false;

    /**
     * Whether the result was reported to the handler for the most recent PIN
     * dialog.
     */
    this.pinDialogResultReported = false;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'loadAuthenticator',
      'doReload',
      'showEnrollmentNudge',
      'showPinDialog',
      'closePinDialog',
      'clickPrimaryButtonForTesting',
      'onBeforeLoad',
      'reset',
      'toggleLoadingUi',
      'setQuickStartEntryPointVisibility',
    ];
  }

  static get observers() {
    return [
      'refreshDialogStep(isShown, pinDialogParameters,' +
          'isLoadingUiShown)',
    ];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return DialogMode.GAIA;
  }

  override get UI_STEPS() {
    return DialogMode;
  }

  private get authenticator(): Authenticator {
    const gaiaDialog =
        this.shadowRoot?.querySelector<GaiaDialog>('#signin-frame-dialog');
    assert(!!gaiaDialog);
    return gaiaDialog.getAuthenticator()!;
  }

  override ready(): void {
    super.ready();
    this.authenticator.insecureContentBlockedCallback =
        this.onInsecureContentBlocked.bind(this);
    this.authenticator.missingGaiaInfoCallback =
        this.missingGaiaInfo.bind(this);
    this.authenticator.samlApiUsedCallback = this.samlApiUsed.bind(this);
    this.authenticator.recordSamlProviderCallback =
        this.recordSamlProvider.bind(this);
    this.authenticator.addEventListener('getDeviceId', () => {
      sendWithPromise('getDeviceIdForLogin')
          .then(deviceId => this.authenticator.getDeviceIdResponse(deviceId));
    });

    this.initializeLoginScreen('GaiaSigninScreen');
  }

  /**
   * Updates whether the Guest and Apps button is allowed to be shown. (Note
   * that the C++ side contains additional logic that decides whether the
   * Guest button should be shown.)
   */
  private isFirstSigninStep(
      uiStep: DialogMode, canGaiaGoBack: boolean, isSaml: boolean): boolean {
    return !this.isClosable && POSSIBLE_FIRST_SIGNIN_STEPS.includes(uiStep) &&
        !canGaiaGoBack && !(isSaml && !this.isDefaultSsoProvider);
  }

  private onIsFirstSigninStepChanged(firstSigninStep: boolean): void {
    if (this.isShown) {
      chrome.send('setIsFirstSigninStep', [firstSigninStep]);
    }
  }

  /**
   * Handles clicks on "Back" button.
   */
  private onBackButtonCancel(): void {
    if (!this.authCompleted) {
      this.cancel(true /* isBackClicked */);
    }
  }

  private onInterstitialBackButtonClicked(): void {
    this.cancel(true /* isBackClicked */);
  }

  /**
   * Handles user closes the dialog on the SAML page.
   */
  private closeSaml(): void {
    this.videoEnabled = false;
    this.cancel(false /* isBackClicked */);
  }

  /**
   * Loads the authenticator and updates the UI to reflect the loading state.
   * @param doSamlRedirect If the authenticator should do
   *     authentication by automatic redirection to the SAML-based enrollment
   *     enterprise domain IdP.
   */
  private loadAuthenticator_(doSamlRedirect: boolean): void {
    this.loadingFrameContents = true;
    this.isDefaultSsoProvider = doSamlRedirect;
    this.startLoadingTimer();

    assert(this.authenticatorParams !== null);
    // Don't enable GAIA action buttons when doing SAML redirect.
    this.authenticatorParams.enableGaiaActionButtons = !doSamlRedirect;
    this.authenticatorParams.doSamlRedirect = doSamlRedirect;
    // Set `isSaml` flag here to make sure we show the correct UI. If we do a
    // redirect, but it fails due to an error, there won't be any `authFlow`
    // change triggered by `authenticator`, however we should still show a
    // button to let user go to GAIA page and keep original GAIA buttons
    // hidden.
    this.isSaml = doSamlRedirect;
    this.authenticator.load(AuthMode.DEFAULT, this.authenticatorParams);
  }

  /**
   * Whether the SAML 3rd-party page is visible.
   */
  private computeSamlSsoVisible(
      isSaml: boolean,
      pinDialogParameters: OobeTypes.SecurityTokenPinDialogParameters):
      boolean {
    return isSaml && !pinDialogParameters;
  }

  /**
   * Handler for Gaia loading timeout.
   */
  private onLoadingTimeOut(): void {
    const currentScreen = Oobe.getInstance().currentScreen;
    if (currentScreen && currentScreen.id !== 'gaia-signin') {
      return;
    }
    this.clearLoadingTimer();
    chrome.send('showLoadingTimeoutError');
  }

  /**
   * Clears loading timer.
   */
  private clearLoadingTimer(): void {
    if (this.loadingTimer) {
      clearTimeout(this.loadingTimer);
      this.loadingTimer = undefined;
    }
  }

  /**
   * Sets up loading timer.
   */
  private startLoadingTimer(): void {
    this.clearLoadingTimer();
    this.loadingTimer = setTimeout(
        this.onLoadingTimeOut.bind(this), MAX_GAIA_LOADING_TIME_SEC * 1000);
  }

  /**
   * Handler for GAIA animation guard timer.
   */
  private onLoadAnimationGuardTimer(): void {
    this.loadAnimationGuardTimer = undefined;
    this.onShowView();
  }

  /**
   * Clears GAIA animation guard timer.
   */
  private clearLoadAnimationGuardTimer(): void {
    if (this.loadAnimationGuardTimer) {
      clearTimeout(this.loadAnimationGuardTimer);
      this.loadAnimationGuardTimer = undefined;
    }
  }

  /**
   * Sets up GAIA animation guard timer.
   */
  private startLoadAnimationGuardTimer(): void {
    this.clearLoadAnimationGuardTimer();
    this.loadAnimationGuardTimer = setTimeout(
        this.onLoadAnimationGuardTimer.bind(this),
        GAIA_ANIMATION_GUARD_MILLISEC);
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.GAIA_SIGNIN;
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param data Screen init payload
   */
  override onBeforeShow(data: GaiaSigninScreenData): void {
    // Re-enable navigation in case it was disabled before refresh.
    this.navigationEnabled = true;

    this.isShown = true;

    if (data && 'hasUserPods' in data) {
      this.isClosable = data.hasUserPods;
    }

    const pinDialog =
        this.shadowRoot?.querySelector<SecurityTokenPin>('#pinDialog');
    assert(!!pinDialog);
    pinDialog.onBeforeShow();

    super.onBeforeShow(data);
  }

  // Used in tests.
  private getSigninFrame(): chrome.webviewTag.WebView {
    const gaiaDialog =
        this.shadowRoot?.querySelector<GaiaDialog>('#signin-frame-dialog');
    assert(!!gaiaDialog);
    return gaiaDialog.getFrame() as chrome.webviewTag.WebView;
  }

  /**
   * Event handler that is invoked just before the screen is hidden.
   */
  override onBeforeHide(): void {
    super.onBeforeHide();
    this.isShown = false;
    this.authenticator.resetWebview();
  }

  /**
   * Loads authenticator.
   * @param data Input for authenticator parameters.
   * @suppress {missingProperties}
   */
  loadAuthenticator(data: any) {
    this.authenticator.setWebviewPartition(data.webviewPartitionName);

    this.authCompleted = false;
    this.navigationButtonsHidden = false;

    // Reset the PIN dialog, in case it's shown.
    this.closePinDialog();

    const params: any = {};
    SUPPORTED_PARAMS.forEach(name => {
      if (data.hasOwnProperty(name)) {
        params[name] = data[name];
      }
    });

    params.doSamlRedirect = data.screenMode === ScreenAuthMode.SAML_REDIRECT;
    params.menuEnterpriseEnrollment =
        !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.isFirstUser = !(data.enterpriseManagedDevice || data.hasDeviceOwner);
    params.obfuscatedOwnerId = data.obfuscatedOwnerId;

    this.authenticatorParams = params;

    this.loadAuthenticator_(params.doSamlRedirect);
    chrome.send('authenticatorLoaded');
  }

  /**
   * Whether the current auth flow is SAML.
   */
  isSamlAuthFlowForTesting(): boolean {
    return this.isSaml && this.authFlow === AuthFlow.SAML;
  }

  /**
   * Clean up from a video-enabled SAML flow.
   */
  private clearVideoTimer(): void {
    if (this.videoTimer !== undefined) {
      clearTimeout(this.videoTimer);
      this.videoTimer = undefined;
    }
  }

  private onVideoEnabledChange(): void {
    if (this.videoEnabled && this.videoTimer === undefined) {
      this.videoTimer = setTimeout(this.cancel.bind(this), VIDEO_LOGIN_TIMEOUT);
    } else {
      this.clearVideoTimer();
    }
  }

  reset() {
    this.clearLoadingTimer();
    this.clearVideoTimer();
    this.authCompleted = false;
    // Reset webview to prevent calls from authenticator.
    this.authenticator.resetWebview();
    this.authenticator.resetStates();
    this.navigationButtonsHidden = true;
    // Explicitly disable video here to let `onVideoEnabledChange()` handle
    // timer start next time when `videoEnabled` will be set to true on SAML
    // page.
    this.videoEnabled = false;
  }

  /**
   * Invoked when the authFlow property is changed on the authenticator.
   */
  private onAuthFlowChange(): void {
    this.isSaml = this.authFlow === AuthFlow.SAML;
  }

  /**
   * Observer that is called when the |isSaml| property gets changed.
   */
  private onSamlChanged(): void {
    chrome.send('samlStateChanged', [this.isSaml]);

    this.classList.toggle('saml', this.isSaml);
  }

  /**
   * Invoked when the authenticator emits 'ready' event or when another
   * authentication frame is completely loaded.
   */
  private onAuthReady(): void {
    this.showViewProcessed = false;
    this.startLoadAnimationGuardTimer();
    this.clearLoadingTimer();
    // Workaround to hide flashing scroll bar.
    setTimeout(() => {
      this.loadingFrameContents = false;
    }, 100);
  }

  private onStartEnrollment(): void {
    this.userActed('startEnrollment');
  }

  /**
   * Invoked when the authenticator emits 'showView' event or when corresponding
   * guard time fires.
   */
  private onShowView(): void {
    if (this.showViewProcessed) {
      return;
    }

    this.showViewProcessed = true;
    this.clearLoadAnimationGuardTimer();
    this.onLoginUiVisible();
  }

  /**
   * Called when UI is shown.
   */
  private onLoginUiVisible(): void {
    chrome.send('loginWebuiReady');
  }

  /**
   * Invoked when the authentication flow had to be aborted because content
   * served over an unencrypted connection was detected. Shows a fatal error.
   * This method is only called on Chrome OS, where the entire authentication
   * flow is required to be encrypted.
   * @param url The URL that was blocked.
   */
  private onInsecureContentBlocked(url: string): void {
    this.showFatalAuthError(
        OobeTypes.FatalErrorCode.INSECURE_CONTENT_BLOCKED, {'url': url});
  }

  /**
   * Shows the fatal auth error.
   * @param errorCode The error code
   * @param info Additional info
   */
  private showFatalAuthError(
      errorCode: OobeTypes.FatalErrorCode, info?: Object): void {
    chrome.send('onFatalError', [errorCode, info || {}]);
  }

  /**
   * Show fatal auth error when information is missing from GAIA.
   */
  private missingGaiaInfo(): void {
    this.showFatalAuthError(OobeTypes.FatalErrorCode.MISSING_GAIA_INFO);
  }

  /**
   * Record that SAML API was used during sign-in.
   * @param isThirdPartyIdP is login flow SAML with external IdP
   */
  private samlApiUsed(isThirdPartyIdP: boolean): void {
    chrome.send('usingSAMLAPI', [isThirdPartyIdP]);
  }

  /**
   * Record SAML Provider that has signed-in
   * @param x509Certificate is a x509certificate in pem format
   */
  private recordSamlProvider(x509Certificate: string): void {
    chrome.send('recordSamlProvider', [x509Certificate]);
  }

  /**
   * Invoked when onAuthCompleted message received.
   * @param e Event with the credentials object as the
   *     payload.
   */
  private onAuthCompletedMessage(e: CustomEvent) {
    const credentials = e.detail;
    if (credentials.publicSAML) {
      this.email = credentials.email;
      chrome.send('launchSAMLPublicSession', [credentials.email]);
    } else {
      chrome.send('completeAuthentication', [
        credentials.gaiaId,
        credentials.email,
        credentials.password,
        credentials.scrapedSAMLPasswords,
        credentials.usingSAML,
        credentials.services,
        credentials.servicesProvided,
        credentials.passwordAttributes,
        credentials.syncTrustedVaultKeys || {},
      ]);
    }

    // Hide the navigation buttons as they are not useful when the loading
    // screen is shown.
    this.navigationButtonsHidden = true;

    this.clearVideoTimer();
    this.authCompleted = true;
  }

  /**
   * Invoked when onLoadAbort message received.
   * @param e Event with the payload containing
   *     additional information about error event like:
   *     {number} error_code Error code such as net::ERR_INTERNET_DISCONNECTED.
   *     {string} src The URL that failed to load.
   */
  private onLoadAbortMessage(e: CustomEvent): void {
    chrome.send('webviewLoadAborted', [e.detail.error_code]);
  }

  /**
   * Invoked when exit message received.
   * @param e Event
   */
  private onExitMessage(): void {
    this.cancel();
  }

  /**
   * Invoked when identifierEntered message received.
   * @param e Event with payload containing:
   *     {string} accountIdentifier User identifier.
   */
  private onIdentifierEnteredMessage(e: CustomEvent): void {
    this.userActed(['identifierEntered', e.detail.accountIdentifier]);
  }

  /**
   * Invoked when removeUserByEmail message received.
   * @param e Event with payload containing:
   *     {string} email User email.
   */
  private onRemoveUserByEmailMessage(e: CustomEvent): void {
    chrome.send('removeUserByEmail', [e.detail]);
    this.cancel();
  }

  /**
   * Reloads authenticator.
   */
  doReload(): void {
    this.authenticator.reload();
    this.loadingFrameContents = true;
    this.startLoadingTimer();
    this.authCompleted = false;
    this.navigationButtonsHidden = false;
  }

  /**
   * Called when user canceled signin. If it is default SAML page we try to
   * close the page. If SAML page was derived from GAIA we return to configured
   * default page (GAIA or default SAML page).
   */
  cancel(isBackClicked: boolean = false): void {
    this.clearVideoTimer();

    // TODO(crbug.com/470893): Figure out whether/which of these exit conditions
    // are useful.
    if (this.authCompleted) {
      return;
    }

    // If user goes back from the derived SAML page we need to reload the
    // default authenticator.
    if (this.isSamlSsoVisible && !this.isDefaultSsoProvider) {
      this.userActed(['reloadGaia', /*force_default_gaia_page*/ false]);
      return;
    }
    this.userActed(isBackClicked ? 'back' : 'cancel');
  }

  /**
   * Show enrollment nudge pop-up.
   * @param domain User's email domain.
   */
  showEnrollmentNudge(domain: string): void {
    this.reset();
    this.emailDomain = domain;
    const enrollmentNudgeDialog =
        this.shadowRoot?.querySelector<OobeModalDialog>('#enrollmentNudge');
    assert(!!enrollmentNudgeDialog);
    enrollmentNudgeDialog.showDialog();
  }

  toggleLoadingUi(isShown: boolean): void {
    this.loadingFrameContents = isShown;
  }

  /**
   * Build localized message to display on enrollment nudge pop-up.
   * @param locale i18n locale data
   * @param domain User's email domain.
   */
  private getEnrollmentNudgeMessage(locale: string, domain: string): string {
    return this.i18nDynamic(locale, 'enrollmentNudgeMessage', domain);
  }

  /**
   * Handler for a button on enrollment nudge pop-up. Should lead the user to
   * reloaded sign in screen.
   */
  private onEnrollmentNudgeUseAnotherAccount(): void {
    this.recordUmaHistogramForEnrollmentNudgeUserAction(
        EnrollmentNudgeUserAction.USE_ANOTHER_ACCOUNT_BUTTON);
    const enrollmentNudgeDialog =
        this.shadowRoot?.querySelector<OobeModalDialog>('#enrollmentNudge');
    assert(!!enrollmentNudgeDialog);
    enrollmentNudgeDialog.hideDialog();
    this.doReload();
  }

  /**
   * Handler for a button on enrollment nudge pop-up. Should switch to
   * enrollment screen.
   */
  private onEnrollmentNudgeEnroll(): void {
    this.recordUmaHistogramForEnrollmentNudgeUserAction(
        EnrollmentNudgeUserAction.ENTERPRISE_ENROLLMENT_BUTTON);
    const enrollmentNudgeDialog =
        this.shadowRoot?.querySelector<OobeModalDialog>('#enrollmentNudge');
    assert(!!enrollmentNudgeDialog);
    enrollmentNudgeDialog.hideDialog();
    this.userActed('startEnrollment');
  }

  /**
   * Shows the PIN dialog according to the given parameters.
   *
   * In case the dialog is already shown, updates it according to the new
   * parameters.
   */
  showPinDialog(parameters: OobeTypes.SecurityTokenPinDialogParameters): void {
    assert(parameters);

    // Note that this must be done before updating |pinDialogResultReported|,
    // since the observer will notify the handler about the cancellation of the
    // previous dialog depending on this flag.
    this.pinDialogParameters = parameters;

    this.pinDialogResultReported = false;
  }

  /**
   * Closes the PIN dialog (that was previously opened using showPinDialog()).
   * Does nothing if the dialog is not shown.
   */
  closePinDialog(): void {
    // Note that the update triggers the observer, that notifies the handler
    // about the closing.
    this.pinDialogParameters = null;
  }

  /**
   * Observer that is called when the |pinDialogParameters| property gets
   * changed.
   */
  private onPinDialogParametersChanged(
      newValue: OobeTypes.SecurityTokenPinDialogParameters,
      oldValue: OobeTypes.SecurityTokenPinDialogParameters): void {
    if (oldValue === undefined) {
      // Don't do anything on the initial call, triggered by the property
      // initialization.
      return;
    }
    if (oldValue === null && newValue !== null) {
      // Asynchronously set the focus, so that this happens after Polymer
      // recalculates the visibility of |pinDialog|.
      // Also notify the C++ test after this happens, in order to avoid
      // flakiness (so that the test doesn't try to simulate the input before
      // the caret is positioned).
      requestAnimationFrame(() => {
        const pinDialog =
            this.shadowRoot?.querySelector<SecurityTokenPin>('#pinDialog');
        if (pinDialog) {
          pinDialog.focus();
        }
        chrome.send('securityTokenPinDialogShownForTest');
      });
    }
    if ((oldValue !== null && newValue === null) ||
        (oldValue !== null && newValue !== null &&
         !this.pinDialogResultReported)) {
      // Report the cancellation result if the dialog got closed or got reused
      // before reporting the result.
      chrome.send('securityTokenPinEntered', [/*user_input=*/ '']);
    }
  }

  /**
   * Invoked when the user cancels the PIN dialog.
   */
  onPinDialogCanceled(): void {
    this.closePinDialog();
    this.cancel();
  }

  /**
   * Invoked when the PIN dialog is completed.
   * @param e Event with the entered PIN as the payload.
   */
  onPinDialogCompleted(e: CustomEvent<string>): void {
    this.pinDialogResultReported = true;
    chrome.send('securityTokenPinEntered', [/*user_input=*/ e.detail]);
  }

  /**
   * Updates current UI step based on internal state.
   */
  private refreshDialogStep(
      isScreenShown: boolean,
      pinParams: OobeTypes.SecurityTokenPinDialogParameters,
      isLoading: boolean): void {
    if (!isScreenShown) {
      return;
    }
    if (pinParams !== null) {
      this.setUIStep(DialogMode.PIN_DIALOG);
      return;
    }
    if (isLoading) {
      this.setUIStep(DialogMode.LOADING);
      return;
    }
    this.setUIStep(DialogMode.GAIA);
  }

  /**
   * Invoked when "Enter Google Account info" button is pressed on SAML screen.
   */
  private onSamlPageChangeAccount() {
    // The user requests to change the account so the default gaia
    // page must be shown.
    this.userActed(['reloadGaia', /*force_default_gaia_page*/ true]);
  }

  /**
   * Computes the value of the isLoadingUiShown property.
   */
  private computeIsLoadingUiShown(
      loadingFrameContents: boolean, authCompleted: boolean): boolean {
    return (loadingFrameContents || authCompleted);
  }

  clickPrimaryButtonForTesting(): void {
    const gaiaDialog =
        this.shadowRoot?.querySelector<GaiaDialog>('#signin-frame-dialog');
    assert(!!gaiaDialog);
    gaiaDialog.clickPrimaryButtonForTesting();
  }

  onBeforeLoad(): void {
    // TODO(https://crbug.com/1317991): Investigate why the call is making Gaia
    // loading slowly.
    // this.loadingFrameContents = true;
  }

  /**
   * Copmose alert message when third-party IdP uses camera for authentication.
   * @param locale  i18n locale data
   */
  private getSamlVideoAlertMessage(
      locale: string, videoEnabled: boolean, authDomain: string): string {
    if (videoEnabled && authDomain) {
      return this.i18nDynamic(locale, 'samlNoticeWithVideo', authDomain);
    }
    return '';
  }

  /**
   * Handle "Quick Start" button for "Signin" screen.
   *
   */
  private onQuickStartButtonClicked(): void {
    this.userActed('activateQuickStart');
  }

  private setQuickStartEntryPointVisibility(visible: boolean): void {
    const gaiaDialog =
        this.shadowRoot?.querySelector<GaiaDialog>('#signin-frame-dialog');
    assert(!!gaiaDialog);
    gaiaDialog.isQuickStartEnabled = visible;
  }

  private recordUmaHistogramForEnrollmentNudgeUserAction(
      userAction: EnrollmentNudgeUserAction): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Enterprise.EnrollmentNudge.UserAction',
      userAction,
      EnrollmentNudgeUserAction.MAX,
    ]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GaiaSigninElement.is]: GaiaSigninElement;
  }
}

customElements.define(GaiaSigninElement.is, GaiaSigninElement);
