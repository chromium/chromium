// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Enterprise Enrollment screen.
 */

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {GaiaDialog} from '../../components/gaia_dialog.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OfflineAdLogin} from '../common/offline_ad_login.js';

import {Authenticator, AuthFlow, AuthMode, AuthParams} from '//oobe/gaia_auth_host/authenticator.js';
import {assert} from '//resources/js/assert.js';
import {sendWithPromise} from '//resources/js/cr.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {InjectedKeyboardUtils} from '../../components/keyboard_utils.js';
import {globalOobeKeyboard, KEYBOARD_UTILS_FOR_INJECTION} from '../../components/keyboard_utils_oobe.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {Oobe} from '../../cr_ui.js';
import * as OobeDebugger from '../../debug/debug.js';
import {invokePolymerMethod} from '../../display_manager.js';
import type {ActiveDirectoryErrorState, JoinConfigType} from '../common/offline_ad_login.js';
import {ADLoginStep} from '../common/offline_ad_login.js';

import {getTemplate} from './enterprise_enrollment.html.js';

const EnterpriseEnrollmentElementBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
    };

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface EnterpriseEnrollmentScreenData {
  enrollment_mode: string;
  is_enrollment_enforced: boolean;
  attestationBased: boolean;
  flow: string;
  license: string|undefined;
  gaiaUrl: string|undefined;
  gaiaPath: string|undefined;
  gaia_buttons_type: string|undefined;
  clientId: string|undefined;
  hl: string|undefined;
  management_domain: string|undefined;
  email: string|undefined;
  webviewPartitionName: string|undefined;
}

declare global {
  interface HTMLElementEventMap {
    'authCompletedAd': CustomEvent<{
      'distinguished_name': string,
      'username': string,
      'password': string,
      'machine_name': string,
      'encryption_types': string,
    }>;
    'unlockPasswordEntered': CustomEvent<{
      'unlock_password': string,
    }>;
  }
}

export class EnterpriseEnrollmentElement extends
    EnterpriseEnrollmentElementBase {
  static get is() {
    return 'enterprise-enrollment-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Manager of the enrolled domain. Either a domain (foo.com) or an isMeet
       * address (admin@foo.com).
       */
      domainManager: {
        type: String,
        value: '',
      },

      /**
       * Name of the device that was enrolled.
       */
      deviceName: {
        type: String,
        value: 'Chromebook',
      },

      /**
       * Type of license used for enrollment.
       * Only relevant for manual (gaia) flow.
       */
      licenseType: {
        type: Number,
        value: OobeTypes.LicenseType.ENTERPRISE,
      },

      /**
       * Type of bottom buttons in the GAIA dialog.
       */
      gaiaDialogButtonsType: {
        type: String,
        value: OobeTypes.GaiaDialogButtonsType.DEFAULT,
      },

      /**
       * Text on the error screen.
       */
      errorText: {
        type: String,
        value: '',
      },

      /**
       * Controls if there will be "Try Again" button on the error screen.
       *
       * True:  Error Nature Recoverable
       * False: Error Nature Fatal
       */
      canRetryAfterError: {
        type: Boolean,
        value: true,
      },

      /**
       * Device attribute : Asset ID.
       */
      assetId: {
        type: String,
        value: '',
      },

      /**
       * Device attribute : Location.
       */
      deviceLocation: {
        type: String,
        value: '',
      },

      /**
       * Whether account identifier should be sent for check.
       */
      hasAccountCheck: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the enrollment is automatic
       *
       * True:  Automatic (Attestation-based)
       * False: Manual (OAuth)
       */
      isAutoEnroll: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the enrollment is enforced and cannot be skipped.
       *
       * True:  Enrollment Enforced
       * False: Enrollment Optional
       */
      isForced: {
        type: Boolean,
        value: false,
      },

      /**
       * Bound to gaia-dialog::authFlow.
       */
      authFlow: {
        type: Number,
      },

      /**
       * Email for enrollment.
       */
      email: {
        type: String,
      },

      isMeet: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('deviceFlowType') &&
              (loadTimeData.getString('deviceFlowType') == 'meet');
        },
        readOnly: true,
      },
    };
  }

  private domainManager: string;
  private deviceName: string;
  private licenseType: OobeTypes.LicenseType;
  private gaiaDialogButtonsType: string;
  private errorText: string;
  private canRetryAfterError: boolean;
  private assetId: string;
  private deviceLocation: string;
  private hasAccountCheck: boolean;
  private isAutoEnroll: boolean;
  private isForced: boolean;
  private authFlow: number;
  private email: string;
  private readonly isMeet: boolean;
  private isCancelDisabled: boolean;
  private isManualEnrollment: boolean|undefined;

  constructor() {
    super();
    /**
     * We block esc, back button and cancel button until gaia is loaded to
     * prevent multiple cancel events.
     */
    this.isCancelDisabled = false;

    this.isManualEnrollment = undefined;
  }

  override get EXTERNAL_API(): string[] {
    return [
      'doReload',
      'setAdJoinConfiguration',
      'setAdJoinParams',
      'setEnterpriseDomainInfo',
      'showAttributePromptStep',
      'showError',
      'showStep',
      'showSkipConfirmationDialog',
    ];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): OobeTypes.EnrollmentStep {
    return OobeTypes.EnrollmentStep.LOADING;
  }

  override get UI_STEPS() {
    return OobeTypes.EnrollmentStep;
  }

  private getGaiaDialog(): GaiaDialog {
    const gaiaDialog =
        this.shadowRoot?.querySelector<GaiaDialog>('#step-signin');
    assert(gaiaDialog instanceof GaiaDialog);
    return gaiaDialog;
  }

  private getOfflineAdLogin(): OfflineAdLogin {
    const offlineAdLogin =
        this.shadowRoot?.querySelector<OfflineAdLogin>('#step-ad-join');
    assert(offlineAdLogin instanceof OfflineAdLogin);
    return offlineAdLogin;
  }

  private getSkipConfirmationDialog(): OobeModalDialog {
    const skipConfirmationDialog =
        this.shadowRoot?.querySelector<OobeModalDialog>(
            '#skipConfirmationDialog');
    assert(skipConfirmationDialog instanceof OobeModalDialog);
    return skipConfirmationDialog;
  }

  get authenticator(): Authenticator {
    return this.getGaiaDialog().getAuthenticator();
  }

  get authView(): chrome.webviewTag.WebView {
    // TODO(b/314762562): remove type cast once gaia_dialog is migrated to TS.
    return this.getGaiaDialog().getFrame() as chrome.webviewTag.WebView;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('OAuthEnrollmentScreen');

    // Establish an initial messaging between content script and
    // host script so that content script can message back.
    this.authView.addEventListener('loadstop', (e: Event) => {
      const target = e.target;
      // Could be null in tests.
      if (target instanceof HTMLIFrameElement && !!target.contentWindow) {
        target.contentWindow.postMessage(
            InjectedKeyboardUtils.INITIAL_MSG, this.authView.src);
      }
    });

    const offlineAdLogin = this.getOfflineAdLogin();
    offlineAdLogin.addEventListener(
        'authCompletedAd', (e: CustomEvent<{
                             'distinguished_name': string,
                             'username': string,
                             'password': string,
                             'machine_name': string,
                             'encryption_types': string,
                           }>) => {
          offlineAdLogin.disabled = true;
          offlineAdLogin.loading = true;
          chrome.send('oauthEnrollAdCompleteLogin', [
            e.detail.machine_name,
            e.detail.distinguished_name,
            e.detail.encryption_types,
            e.detail.username,
            e.detail.password,
          ]);
        });

    offlineAdLogin.addEventListener(
        'unlockPasswordEntered', (e: CustomEvent<{
                                   'unlock_password': string,
                                 }>) => {
          offlineAdLogin.disabled = true;
          chrome.send(
              'oauthEnrollAdUnlockConfiguration', [e.detail.unlock_password]);
        });

    this.authenticator.insecureContentBlockedCallback = (url: string) => {
      this.showError(
          loadTimeData.getStringF('insecureURLEnrollmentError', url), false);
    };

    this.authenticator.missingGaiaInfoCallback = () => {
      this.showError(loadTimeData.getString('fatalEnrollmentError'), false);
    };

    this.authenticator.addEventListener('getDeviceId', (_e: Event) => {
      sendWithPromise('getDeviceIdForEnrollment')
          .then(deviceId => this.authenticator.getDeviceIdResponse(deviceId));
    });
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param data Screen init payload,
   * contains the signin frame URL.
   */
  onBeforeShow(data?: EnterpriseEnrollmentScreenData): void {
    if (data === undefined) {
      return;
    }

    if (Oobe.getInstance().forceKeyboardFlow) {
      assert(KEYBOARD_UTILS_FOR_INJECTION.DATA);
      globalOobeKeyboard.enableHandlingOfInjectedKeyboardUtilsMessages();
      // We run the tab remapping logic inside of the webview so that the
      // simulated tab events will use the webview tab-stops. Simulated tab
      // events created from the webui treat the entire webview as one tab
      // stop. Real tab events do not do this. See crbug.com/543865.
      this.authView.addContentScripts([{
        name: 'injectedTabHandler',
        matches: ['http://*/*', 'https://*/*'],
        js: {code: KEYBOARD_UTILS_FOR_INJECTION.DATA},
        run_at: 'document_start' as chrome.extensionTypes.RunAt,
      }]);
    }

    this.isManualEnrollment = (data.enrollment_mode === 'manual');
    this.isForced = data.is_enrollment_enforced;
    this.isAutoEnroll = data.attestationBased;
    this.hasAccountCheck =
        ((data.flow === 'enterpriseLicense') ||
         (data.flow === 'educationLicense'));

    this.licenseType = data.license ? this.convertLicenseType(data.license) :
                                      OobeTypes.LicenseType.ENTERPRISE;

    if (!this.isAutoEnroll) {
      const gaiaParams = {} as AuthParams;
      // If `isAutoEnroll` is false subsequent fields should exist.
      gaiaParams.gaiaUrl = data.gaiaUrl!;
      gaiaParams.gaiaPath = data.gaiaPath!;
      gaiaParams.clientId = data.clientId!;
      gaiaParams.needPassword = false;
      gaiaParams.hl = data.hl!;
      if (data.management_domain) {
        gaiaParams.enterpriseEnrollmentDomain = data.management_domain;
        gaiaParams.emailDomain = data.management_domain;
      }
      gaiaParams.flow = data.flow;
      gaiaParams.enableGaiaActionButtons = true;
      if (data.email) {
        // TODO(b/292087570): we have to set `readOnlyEmail` even though email
        // will in fact be modifiable.
        gaiaParams.readOnlyEmail = true;
        gaiaParams.email = data.email;
      }

      this.authenticator.setWebviewPartition(
          data.webviewPartitionName ? data.webviewPartitionName : '');

      this.authenticator.load(AuthMode.DEFAULT, gaiaParams);

      if (data.gaia_buttons_type) {
        this.gaiaDialogButtonsType = data.gaia_buttons_type;
      }
      if (this.gaiaDialogButtonsType ==
          OobeTypes.GaiaDialogButtonsType.KIOSK_PREFERRED) {
        this.licenseType = OobeTypes.LicenseType.KIOSK;
      }
    }

    invokePolymerMethod(this.getOfflineAdLogin(), 'onBeforeShow');
    if (!this.uiStep) {
      this.showStep(
          this.isAutoEnroll ? OobeTypes.EnrollmentStep.WORKING :
                              OobeTypes.EnrollmentStep.LOADING);
    }
  }

  /**
   * Initial UI State for screen
   */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OOBE_UI_STATE {
    return OOBE_UI_STATE.ENROLLMENT_CANCEL_DISABLED;
  }

  /**
   * Shows attribute-prompt step with pre-filled asset ID and
   * location.
   */
  showAttributePromptStep(annotatedAssetId: string, annotatedLocation: string):
      void {
    this.assetId = annotatedAssetId;
    this.deviceLocation = annotatedLocation;
    this.showStep(OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT);
  }

  /**
   * Sets the type of the device and the enterprise domain to be shown.
   *
   */
  setEnterpriseDomainInfo(manager: string, deviceType: string): void {
    this.domainManager = manager;
    this.deviceName = deviceType;
  }

  /**
   * Invoked when identifierEntered message received.
   * @param e Event with payload
   *     containing: {string} accountIdentifier User identifier.
   */
  private onIdentifierEnteredMessage(
      e: CustomEvent<{'accountIdentifier': string}>): void {
    if (this.hasAccountCheck) {
      this.showStep(OobeTypes.EnrollmentStep.CHECKING);
      chrome.send('enterpriseIdentifierEntered', [e.detail.accountIdentifier]);
    }
  }

  /**
   * Cancels the current authentication and drops the user back to the next
   * screen (either the next authentication or the login screen).
   */
  private cancel(): void {
    if (this.isCancelDisabled) {
      return;
    }
    this.isCancelDisabled = true;
    this.closeEnrollment('cancel');
  }

  /**
   * Switches between the different steps in the enrollment flow.
   * @param step the steps to show, one of "signin", "working",
   * "attribute-prompt", "error", "success".
   */
  showStep(step: OobeTypes.EnrollmentStep): void {
    this.setUIStep(step);
    if (step === OobeTypes.EnrollmentStep.AD_JOIN) {
      const offlineAdLogin = this.getOfflineAdLogin();
      offlineAdLogin.disabled = false;
      offlineAdLogin.loading = false;
      offlineAdLogin.focus();
    }
    this.isCancelDisabled = (step === OobeTypes.EnrollmentStep.SIGNIN &&
                             !this.isManualEnrollment) ||
        step === OobeTypes.EnrollmentStep.AD_JOIN ||
        step === OobeTypes.EnrollmentStep.WORKING ||
        step === OobeTypes.EnrollmentStep.CHECKING ||
        step === OobeTypes.EnrollmentStep.TPM_CHECKING ||
        step === OobeTypes.EnrollmentStep.LOADING;
    // TODO(b/238175743) Do not set `ENROLLMENT_CANCEL_ENABLED` if enrollment is
    // forced. Keep setting `isCancelDisabled` to false if enrollment is forced,
    // otherwise the manual fallback button does nothing.
    if (this.isCancelDisabled ||
        step === OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT) {
      Oobe.getInstance().setOobeUIState(
          OOBE_UI_STATE.ENROLLMENT_CANCEL_DISABLED);
    } else {
      Oobe.getInstance().setOobeUIState(
          step === OobeTypes.EnrollmentStep.SUCCESS ?
              OOBE_UI_STATE.ENROLLMENT_SUCCESS :
              OOBE_UI_STATE.ENROLLMENT_CANCEL_ENABLED);
    }
  }

  doReload(): void {
    this.authenticator.reload();
  }

  /**
   * Sets Active Directory join screen params.
   * @param showUnlockConfig true if there is an encrypted
   * configuration (and not unlocked yet).
   */
  setAdJoinParams(
      machineName: string, userName: string,
      errorState: ActiveDirectoryErrorState, showUnlockConfig: boolean): void {
    const offlineAdLogin = this.getOfflineAdLogin();
    offlineAdLogin.disabled = false;
    offlineAdLogin.machineName = machineName;
    offlineAdLogin.userName = userName;
    offlineAdLogin.errorState = errorState;
    if (showUnlockConfig) {
      offlineAdLogin.setUIStep(ADLoginStep.UNLOCK);
    } else {
      offlineAdLogin.setUIStep(ADLoginStep.CREDS);
    }
  }

  /**
   * Sets Active Directory join screen with the unlocked configuration.
   */
  setAdJoinConfiguration(options: JoinConfigType[]): void {
    const offlineAdLogin = this.getOfflineAdLogin();
    offlineAdLogin.disabled = false;
    offlineAdLogin.setJoinConfigurationOptions(options);
    offlineAdLogin.setUIStep(ADLoginStep.CREDS);
    offlineAdLogin.focus();
  }

  clickPrimaryButtonForTesting(): void {
    this.getGaiaDialog().clickPrimaryButtonForTesting();
  }

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  private skipAttributes(): void {
    this.showStep(OobeTypes.EnrollmentStep.SUCCESS);
  }

  /**
   * Uploads the device attributes to server. This goes to C++ side through
   * |chrome| and launches the device attribute update negotiation.
   */
  private submitAttributes(): void {
    chrome.send('oauthEnrollAttributes', [this.assetId, this.deviceLocation]);
  }

  /**
   * Shows the learn more dialog.
   */
  private onLearnMore(): void {
    chrome.send('oauthEnrollOnLearnMore');
  }

  private closeEnrollment(result: string): void {
    chrome.send('oauthEnrollClose', [result]);
  }

  private onCancelKiosk(): void {
    this.doReload();
    this.showStep(OobeTypes.EnrollmentStep.SIGNIN);
  }

  private onEnrollKiosk(): void {
    chrome.send(
        'oauthEnrollCompleteLogin', [this.email, OobeTypes.LicenseType.KIOSK]);
  }

  /**
   * Notifies chrome that enrollment have finished.
   */
  private onEnrollmentFinished(): void {
    this.closeEnrollment('done');
  }

  /**
   * Generates message on the success screen.
   */
  private successText(locale: string, device: string, domain: string): string {
    return this.i18nAdvancedDynamic(
        locale, 'oauthEnrollAbeSuccessDomain',
        {substitutions: [device, domain]});
  }

  private isEmpty(str: string): boolean {
    return !str;
  }

  private onAuthCompleted(e: CustomEvent): void {
    const detail = e.detail;
    if (!detail.email) {
      this.showError(loadTimeData.getString('fatalEnrollmentError'), false);
      return;
    }
    if (this.licenseType == OobeTypes.LicenseType.ENTERPRISE) {
      chrome.send(
          'oauthEnrollCompleteLogin',
          [detail.email, OobeTypes.LicenseType.ENTERPRISE]);
    } else if (this.licenseType == OobeTypes.LicenseType.EDUCATION) {
      chrome.send(
          'oauthEnrollCompleteLogin',
          [detail.email, OobeTypes.LicenseType.EDUCATION]);
    } else {
      this.email = detail.email;
      this.showStep(OobeTypes.EnrollmentStep.KIOSK_ENROLLMENT);
    }
  }

  private onReady(): void {
    if (this.uiStep == OobeTypes.EnrollmentStep.LOADING) {
      this.showStep(OobeTypes.EnrollmentStep.SIGNIN);
    }
    if (this.uiStep != OobeTypes.EnrollmentStep.SIGNIN) {
      return;
    }
    this.isCancelDisabled = false;

    if (this.openedFromDebugOverlay()) {
      return;
    }
    chrome.send('frameLoadingCompleted');
  }

  private openedFromDebugOverlay(): boolean {
    if (OobeDebugger.DebuggerUI &&
        OobeDebugger.DebuggerUI.getInstance().currentScreenId ===
            'enterprise-enrollment') {
      console.warn(
          'Enrollment screen was opened using debug overlay: ' +
          'omit chrome.send() to prevent calls on C++ side.');
      return true;
    }
    return false;
  }

  private onLicenseTypeSelected(e: CustomEvent): void {
    this.licenseType = e.detail;
  }

  /**
   * ERROR DIALOG LOGIC:
   *
   *    The error displayed on the enrollment error dialog depends on the nature
   *    of the error (_recoverable_/_fatal_), on the authentication mechanism
   *    (_manual_/_automatic_), and on whether the enrollment is _enforced_ or
   *    _optional_.
   *
   *    AUTH MECH |  ENROLLMENT |  ERROR NATURE            Buttons Layout
   *    ----------------------------------------
   *    AUTOMATIC |   ENFORCED  |  RECOVERABLE    [    [Enroll Man.][Try Again]]
   *    AUTOMATIC |   ENFORCED  |  FATAL          [               [Enroll Man.]]
   *    AUTOMATIC |   OPTIONAL  |  RECOVERABLE    [    [Enroll Man.][Try Again]]
   *    AUTOMATIC |   OPTIONAL  |  FATAL          [               [Enroll Man.]]
   *
   *    MANUAL    |   ENFORCED  |  RECOVERABLE    [[Back]           [Try Again]]
   *    MANUAL    |   ENFORCED  |  FATAL          [[Back]                      ]
   *    MANUAL    |   OPTIONAL  |  RECOVERABLE    [           [Skip][Try Again]]
   *    MANUAL    |   OPTIONAL  |  FATAL          [                      [Skip]]
   *
   *    -  The buttons [Back], [Enroll Manually] and [Skip] all call 'cancel'.
   *    - [Enroll Manually] and [Skip] are the same button (GENERIC CANCEL) and
   *      are relabeled depending on the situation.
   *    - [Back] is only shown the button "GENERIC CANCEL" above isn't shown.
   */

  /**
   * Sets an error message and switches to the error screen.
   * @param message the error message.
   * @param retry whether the retry link should be shown.
   */
  showError(message: string, retry: boolean): void {
    this.errorText = message;
    this.canRetryAfterError = retry;

    if (this.uiStep === OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT) {
      this.showStep(OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT_ERROR);
    } else if (this.uiStep === OobeTypes.EnrollmentStep.AD_JOIN) {
      this.showStep(OobeTypes.EnrollmentStep.ACTIVE_DIRECTORY_JOIN_ERROR);
    } else {
      this.showStep(OobeTypes.EnrollmentStep.ERROR);
    }
  }

  private convertLicenseType(license: string): OobeTypes.LicenseType {
    switch (license) {
      case 'enterprise':
        return OobeTypes.LicenseType.ENTERPRISE;
      case 'education':
        return OobeTypes.LicenseType.EDUCATION;
      case 'terminal':
        return OobeTypes.LicenseType.KIOSK;
      default:
        return OobeTypes.LicenseType.NONE;
    }
  }

  /**
   *  Provides the label for the generic cancel button (Skip / Enroll Manually)
   *  During automatic enrollment, the label is 'Enroll Manually'.
   *  During manual enrollment, the label is 'Skip'.
   */
  private getCancelButtonLabel(isAutomatic: boolean): string {
    return isAutomatic ? 'oauthEnrollManualEnrollment' : 'oauthEnrollSkip';
  }

  /**
   * Return title for enrollment in progress screen.
   */
  private getWorkingTitleKey(licenseType: OobeTypes.LicenseType): string {
    if (licenseType == OobeTypes.LicenseType.ENTERPRISE) {
      return 'oauthEnrollScreenTitle';
    }
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return 'oauthEducationEnrollScreenTitle';
    }
    return 'oauthEnrollKioskEnrollmentWorkingTitle';
  }

  /**
   * Returns icon for enrollment steps.
   */
  private getIcon(licenseType: OobeTypes.LicenseType): string {
    if (licenseType == OobeTypes.LicenseType.ENTERPRISE) {
      return 'oobe-32:enterprise';
    }
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return 'oobe-32:enterprise';
    }
    return 'oobe-32:kiosk';
  }

  /**
   * Return title for success enrollment screen.
   */
  private getSuccessTitle(locale: string, licenseType: OobeTypes.LicenseType):
      string {
    if (licenseType == OobeTypes.LicenseType.ENTERPRISE) {
      return this.i18nDynamic(locale, 'oauthEnrollSuccessTitle');
    }
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18nDynamic(locale, 'oauthEnrollEducationSuccessTitle');
    }
    return this.i18nDynamic(locale, 'oauthEnrollKioskEnrollmentSuccessTitle');
  }

  /**
   * Return title for error enrollment screen.
   */
  private getErrorTitle(locale: string, licenseType: OobeTypes.LicenseType):
      string {
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18nDynamic(locale, 'oauthEducationEnrollErrorTitle');
    }
    return this.i18nDynamic(locale, 'oauthEnrollErrorTitle');
  }

  /**
   *  Whether the "GENERIC CANCEL" (SKIP / ENROLL_MANUALLY ) button should be
   *  shown. It is only shown when in 'AUTOMATIC' mode OR when in
   *  manual mode without enrollment enforcement.
   *
   *  When the enrollment is manual AND forced, a 'BACK' button will be shown.
   * @param automatic - Whether the enrollment is automatic
   * @param enforced  - Whether the enrollment is enforced
   */
  private isGenericCancel(automatic: boolean, enforced: boolean): boolean {
    return automatic || (!automatic && !enforced);
  }

  /**
   * Retries the enrollment process after an error occurred in a previous
   * attempt. This goes to the C++ side through |chrome| first to clean up the
   * profile, so that the next attempt is performed with a clean state.
   */
  private doRetry(): void {
    chrome.send('oauthEnrollRetry');
  }

  /**
   *  Event handler for the 'Try again' button that is shown upon an error
   *  during ActiveDirectory join.
   */
  private onAdJoinErrorRetry(): void {
    this.showStep(OobeTypes.EnrollmentStep.AD_JOIN);
  }

  /*
   * Whether authFlow is the SAML.
   */
  isSaml(authFlow: AuthFlow): boolean {
    return authFlow === AuthFlow.SAML;
  }

  /*
   * Called when we cancel TPM check early.
   */
  private onTpmCheckCanceled(): void {
    this.userActed('cancel-tpm-check');
  }

  // Skip enrollment dialogue section.

  /**
   * Return title for skip enrollment dialogue.
   */
  private getSkipConfirmationTitle(
      locale: string, licenseType: OobeTypes.LicenseType): string {
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18nDynamic(locale, 'skipConfirmationDialogEducationTitle');
    }
    return this.i18nDynamic(locale, 'skipConfirmationDialogTitle');
  }

  /**
   * Return text for skip enrollment dialogue.
   */
  private getSkipConfirmationText(
      locale: string, licenseType: OobeTypes.LicenseType): string {
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18nDynamic(locale, 'skipConfirmationDialogEducationText');
    }
    return this.i18nDynamic(locale, 'skipConfirmationDialogText');
  }

  /*
   * Called when we click go back button.
   */
  private onDialogClosed(): void {
    this.getSkipConfirmationDialog().hideDialog();
  }

  /*
   * Called when we click skip button.
   */
  private onDialogSkip(): void {
    this.getSkipConfirmationDialog().hideDialog();
    this.userActed('skip-confirmation');
  }

  showSkipConfirmationDialog(): void {
    this.getSkipConfirmationDialog().showDialog();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [EnterpriseEnrollmentElement.is]: EnterpriseEnrollmentElement;
  }
}

customElements.define(
    EnterpriseEnrollmentElement.is, EnterpriseEnrollmentElement);
