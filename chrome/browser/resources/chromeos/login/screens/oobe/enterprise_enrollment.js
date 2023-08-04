// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Enterprise Enrollment screen.
 */

import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/gaia_dialog.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {assert} from '//resources/ash/common/assert.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, Polymer, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Authenticator, AuthFlow, AuthMode, AuthParams, SUPPORTED_PARAMS} from '../../../../gaia_auth_host/authenticator.js';
import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeBackButton} from '../../components/buttons/oobe_back_button.js';
import {OobeNextButton} from '../../components/buttons/oobe_next_button.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OOBE_UI_STATE, SCREEN_GAIA_SIGNIN} from '../../components/display_manager_types.js';
import {InjectedKeyboardUtils} from '../../components/keyboard_utils.js';
import {globalOobeKeyboard, KEYBOARD_UTILS_FOR_INJECTION} from '../../components/keyboard_utils_oobe.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {Oobe} from '../../cr_ui.js';
import {DisplayManager, invokePolymerMethod} from '../../display_manager.js';
import {ActiveDirectoryErrorState, ADLoginStep, JoinConfigType} from '../common/offline_ad_login.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const EnterpriseEnrollmentElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

/**
 * @typedef {{
 *   skipConfirmationDialog:  OobeModalDialog,
 * }}
 */
EnterpriseEnrollmentElementBase.$;

/**
 * @polymer
 */
class EnterpriseEnrollmentElement extends EnterpriseEnrollmentElementBase {
  static get is() {
    return 'enterprise-enrollment-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Manager of the enrolled domain. Either a domain (foo.com) or an email
       * address (admin@foo.com).
       */
      domainManager_: {
        type: String,
        value: '',
      },

      /**
       * Name of the device that was enrolled.
       */
      deviceName_: {
        type: String,
        value: 'Chromebook',
      },

      /**
       * Type of license used for enrollment.
       * Only relevant for manual (gaia) flow.
       */
      licenseType_: {
        type: Number,
        value: OobeTypes.LicenseType.ENTERPRISE,
      },

      /**
       * Type of bottom buttons in the GAIA dialog.
       */
      gaiaDialogButtonsType_: {
        type: String,
        value: OobeTypes.GaiaDialogButtonsType.DEFAULT,
      },

      /**
       * Text on the error screen.
       */
      errorText_: {
        type: String,
        value: '',
      },

      /**
       * Controls if there will be "Try Again" button on the error screen.
       *
       * True:  Error Nature Recoverable
       * False: Error Nature Fatal
       */
      canRetryAfterError_: {
        type: Boolean,
        value: true,
      },

      /**
       * Device attribute : Asset ID.
       */
      assetId_: {
        type: String,
        value: '',
      },

      /**
       * Device attribute : Location.
       */
      deviceLocation_: {
        type: String,
        value: '',
      },

      /**
       * Whether account identifier should be sent for check.
       */
      hasAccountCheck_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the enrollment is automatic
       *
       * True:  Automatic (Attestation-based)
       * False: Manual (OAuth)
       */
      isAutoEnroll_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the enrollment is enforced and cannot be skipped.
       *
       * True:  Enrollment Enforced
       * False: Enrollment Optional
       */
      isForced_: {
        type: Boolean,
        value: false,
      },

      /**
       * Bound to gaia-dialog::authFlow.
       * @private
       */
      authFlow_: {
        type: Number,
      },

      /**
       * Email for enrollment.
       * @private
       */
      email_: {
        type: String,
      },

      isMeet_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('deviceFlowType') &&
              (loadTimeData.getString('deviceFlowType') == 'meet');
        },
        readOnly: true,
      },
    };
  }

  constructor() {
    super();
    /**
     * We block esc, back button and cancel button until gaia is loaded to
     * prevent multiple cancel events.
     * @type {?boolean}
     */
    this.isCancelDisabled = false;

    this.isManualEnrollment_ = undefined;
  }

  get EXTERNAL_API() {
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

  defaultUIStep() {
    return OobeTypes.EnrollmentStep.LOADING;
  }

  get UI_STEPS() {
    return OobeTypes.EnrollmentStep;
  }

  get authenticator_() {
    return this.$['step-signin'].getAuthenticator();
  }

  get authView_() {
    return this.$['step-signin'].getFrame();
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('OAuthEnrollmentScreen');

    // Establish an initial messaging between content script and
    // host script so that content script can message back.
    this.authView_.addEventListener('loadstop', (e) => {
      // Could be null in tests.
      if (e.target && e.target.contentWindow) {
        e.target.contentWindow.postMessage(
            InjectedKeyboardUtils.INITIAL_MSG, this.authView_.src);
      }
    });

    this.$['step-ad-join'].addEventListener('authCompleted', (e) => {
      this.$['step-ad-join'].disabled = true;
      this.$['step-ad-join'].loading = true;
      chrome.send('oauthEnrollAdCompleteLogin', [
        e.detail.machine_name,
        e.detail.distinguished_name,
        e.detail.encryption_types,
        e.detail.username,
        e.detail.password,
      ]);
    });

    this.$['step-ad-join'].addEventListener('unlockPasswordEntered', (e) => {
      this.$['step-ad-join'].disabled = true;
      chrome.send(
          'oauthEnrollAdUnlockConfiguration', [e.detail.unlock_password]);
    });

    this.authenticator_.insecureContentBlockedCallback = (url) => {
      this.showError(
          loadTimeData.getStringF('insecureURLEnrollmentError', url), false);
    };

    this.authenticator_.missingGaiaInfoCallback = () => {
      this.showError(loadTimeData.getString('fatalEnrollmentError'), false);
    };
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload, contains the signin frame
   * URL.
   */
  onBeforeShow(data) {
    if (data == undefined) {
      return;
    }

    if (Oobe.getInstance().forceKeyboardFlow) {
      assert(KEYBOARD_UTILS_FOR_INJECTION.DATA);
      globalOobeKeyboard.enableHandlingOfInjectedKeyboardUtilsMessages();
      // We run the tab remapping logic inside of the webview so that the
      // simulated tab events will use the webview tab-stops. Simulated tab
      // events created from the webui treat the entire webview as one tab
      // stop. Real tab events do not do this. See crbug.com/543865.
      this.authView_.addContentScripts([{
        name: 'injectedTabHandler',
        matches: ['http://*/*', 'https://*/*'],
        js: {code: KEYBOARD_UTILS_FOR_INJECTION.DATA},
        run_at: 'document_start',
      }]);
    }

    this.isManualEnrollment_ = 'enrollment_mode' in data ?
        data.enrollment_mode === 'manual' :
        undefined;
    this.isForced_ = 'is_enrollment_enforced' in data ?
        data.is_enrollment_enforced :
        undefined;
    this.isAutoEnroll_ =
        'attestationBased' in data ? data.attestationBased : undefined;
    this.hasAccountCheck_ = 'flow' in data ?
        ((data.flow === 'enterpriseLicense') ||
         (data.flow === 'educationLicense')) :
        false;

    this.licenseType_ = ('license' in data) ?
        this.convertLicenseType(data.license) :
        OobeTypes.LicenseType.ENTERPRISE;

    if (!this.isAutoEnroll_) {
      const gaiaParams = {};
      gaiaParams.gaiaUrl = data.gaiaUrl;
      gaiaParams.clientId = data.clientId;
      gaiaParams.needPassword = false;
      gaiaParams.hl = data.hl;
      if (data.management_domain) {
        gaiaParams.enterpriseEnrollmentDomain = data.management_domain;
        gaiaParams.emailDomain = data.management_domain;
      }
      gaiaParams.flow = data.flow;
      gaiaParams.enableGaiaActionButtons = true;

      this.authenticator_.setWebviewPartition(
          'webviewPartitionName' in data ? data.webviewPartitionName : '');

      this.authenticator_.load(AuthMode.DEFAULT, gaiaParams);

      if (data.gaia_buttons_type) {
        this.gaiaDialogButtonsType_ = data.gaia_buttons_type;
      }
      if (this.gaiaDialogButtonsType_ ==
          OobeTypes.GaiaDialogButtonsType.KIOSK_PREFERRED) {
        this.licenseType_ = OobeTypes.LicenseType.KIOSK;
      }
    }

    invokePolymerMethod(this.$['step-ad-join'], 'onBeforeShow');
    if (!this.uiStep) {
      this.showStep(
          this.isAutoEnroll_ ? OobeTypes.EnrollmentStep.WORKING :
                               OobeTypes.EnrollmentStep.LOADING);
    }
  }

  /**
   * Initial UI State for screen
   */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ENROLLMENT_CANCEL_DISABLED;
  }

  /**
   * Shows attribute-prompt step with pre-filled asset ID and
   * location.
   */
  showAttributePromptStep(annotatedAssetId, annotatedLocation) {
    this.assetId_ = annotatedAssetId;
    this.deviceLocation_ = annotatedLocation;
    this.showStep(OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT);
  }

  /**
   * Sets the type of the device and the enterprise domain to be shown.
   *
   * @param {string} manager
   * @param {string} device_type
   */
  setEnterpriseDomainInfo(manager, device_type) {
    this.domainManager_ = manager;
    this.deviceName_ = device_type;
  }

  /**
   * Invoked when identifierEntered message received.
   * @param {!CustomEvent<!{accountIdentifier: string}>} e Event with payload
   *     containing: {string} accountIdentifier User identifier.
   * @private
   */
  onIdentifierEnteredMessage_(e) {
    if (this.hasAccountCheck_) {
      this.showStep(OobeTypes.EnrollmentStep.CHECKING);
      chrome.send('enterpriseIdentifierEntered', [e.detail.accountIdentifier]);
    }
  }

  /**
   * Cancels the current authentication and drops the user back to the next
   * screen (either the next authentication or the login screen).
   */
  cancel() {
    if (this.isCancelDisabled) {
      return;
    }
    this.isCancelDisabled = true;
    this.closeEnrollment_('cancel');
  }

  /**
   * Switches between the different steps in the enrollment flow.
   * @param {string} step the steps to show, one of "signin", "working",
   * "attribute-prompt", "error", "success".
   * @suppress {missingProperties} setOobeUIState() exists
   */
  showStep(step) {
    this.setUIStep(step);
    if (step === OobeTypes.EnrollmentStep.AD_JOIN) {
      this.$['step-ad-join'].disabled = false;
      this.$['step-ad-join'].loading = false;
      this.$['step-ad-join'].focus();
    }
    this.isCancelDisabled = (step === OobeTypes.EnrollmentStep.SIGNIN &&
                             !this.isManualEnrollment_) ||
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

  doReload() {
    this.authenticator_.reload();
  }

  /**
   * Sets Active Directory join screen params.
   * @param {string} machineName
   * @param {string} userName
   * @param {ActiveDirectoryErrorState} errorState
   * @param {boolean} showUnlockConfig true if there is an encrypted
   * configuration (and not unlocked yet).
   */
  setAdJoinParams(machineName, userName, errorState, showUnlockConfig) {
    this.$['step-ad-join'].disabled = false;
    this.$['step-ad-join'].machineName = machineName;
    this.$['step-ad-join'].userName = userName;
    this.$['step-ad-join'].errorState = errorState;
    if (showUnlockConfig) {
      this.$['step-ad-join'].setUIStep(ADLoginStep.UNLOCK);
    } else {
      this.$['step-ad-join'].setUIStep(ADLoginStep.CREDS);
    }
  }

  /**
   * Sets Active Directory join screen with the unlocked configuration.
   * @param {Array<JoinConfigType>} options
   */
  setAdJoinConfiguration(options) {
    this.$['step-ad-join'].disabled = false;
    this.$['step-ad-join'].setJoinConfigurationOptions(options);
    this.$['step-ad-join'].setUIStep(ADLoginStep.CREDS);
    this.$['step-ad-join'].focus();
  }

  clickPrimaryButtonForTesting() {
    this.$['step-signin'].clickPrimaryButtonForTesting();
  }

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  skipAttributes_() {
    this.showStep(OobeTypes.EnrollmentStep.SUCCESS);
  }

  /**
   * Uploads the device attributes to server. This goes to C++ side through
   * |chrome| and launches the device attribute update negotiation.
   */
  submitAttributes_() {
    chrome.send('oauthEnrollAttributes', [this.assetId_, this.deviceLocation_]);
  }

  /**
   * Shows the learn more dialog.
   */
  onLearnMore_() {
    chrome.send('oauthEnrollOnLearnMore');
  }

  closeEnrollment_(result) {
    chrome.send('oauthEnrollClose', [result]);
  }

  onCancelKiosk_() {
    this.doReload();
    this.showStep(OobeTypes.EnrollmentStep.SIGNIN);
  }

  onEnrollKiosk_() {
    chrome.send(
        'oauthEnrollCompleteLogin', [this.email_, OobeTypes.LicenseType.KIOSK]);
  }

  /**
   * Notifies chrome that enrollment have finished.
   */
  onEnrollmentFinished_() {
    this.closeEnrollment_('done');
  }

  /**
   * Generates message on the success screen.
   */
  successText_(locale, device, domain) {
    return this.i18nAdvanced(
        'oauthEnrollAbeSuccessDomain', {substitutions: [device, domain]});
  }

  isEmpty_(str) {
    return !str;
  }

  onAuthCompleted_(e) {
    var detail = e.detail;
    if (!detail.email) {
      this.showError(loadTimeData.getString('fatalEnrollmentError'), false);
      return;
    }
    if (this.licenseType_ == OobeTypes.LicenseType.ENTERPRISE) {
      chrome.send(
          'oauthEnrollCompleteLogin',
          [detail.email, OobeTypes.LicenseType.ENTERPRISE]);
    } else if (this.licenseType_ == OobeTypes.LicenseType.EDUCATION) {
      chrome.send(
          'oauthEnrollCompleteLogin',
          [detail.email, OobeTypes.LicenseType.EDUCATION]);
    } else {
      this.email_ = detail.email;
      this.showStep(OobeTypes.EnrollmentStep.KIOSK_ENROLLMENT);
    }
  }

  onReady() {
    if (this.uiStep == OobeTypes.EnrollmentStep.LOADING) {
      this.showStep(OobeTypes.EnrollmentStep.SIGNIN);
    }
    if (this.uiStep != OobeTypes.EnrollmentStep.SIGNIN) {
      return;
    }
    this.isCancelDisabled = false;
    chrome.send('frameLoadingCompleted');
  }

  onLicenseTypeSelected(e) {
    this.licenseType_ = e.detail;
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
   * @param {string} message the error message.
   * @param {boolean} retry whether the retry link should be shown.
   */
  showError(message, retry) {
    this.errorText_ = message;
    this.canRetryAfterError_ = retry;

    if (this.uiStep === OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT) {
      this.showStep(OobeTypes.EnrollmentStep.ATTRIBUTE_PROMPT_ERROR);
    } else if (this.uiStep === OobeTypes.EnrollmentStep.AD_JOIN) {
      this.showStep(OobeTypes.EnrollmentStep.ACTIVE_DIRECTORY_JOIN_ERROR);
    } else {
      this.showStep(OobeTypes.EnrollmentStep.ERROR);
    }
  }


  convertLicenseType(license) {
    switch (license) {
      case 'enterprise':
        return OobeTypes.LicenseType.ENTERPRISE;
      case 'education':
        return OobeTypes.LicenseType.EDUCATION;
      case 'terminal':
        return OobeTypes.LicenseType.KIOSK;
    }
  }

  /**
   *  Provides the label for the generic cancel button (Skip / Enroll Manually)
   *
   *  During automatic enrollment, the label is 'Enroll Manually'.
   *  During manual enrollment, the label is 'Skip'.
   * @private
   */
  getCancelButtonLabel_(locale_, is_automatic) {
    if (this.isAutoEnroll_) {
      return 'oauthEnrollManualEnrollment';
    } else {
      return 'oauthEnrollSkip';
    }
  }

  /**
   * Return title for enrollment in progress screen.
   * @param {string} licenseType
   * @returns {string}
   * @private
   */
  getWorkingTitleKey_(licenseType) {
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
   * @param {*} licenseType
   * @returns {string}
   * @private
   */
  getIcon_(licenseType) {
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
   * @param {string} licenseType
   * @returns {string}
   * @private
   */
  getSuccessTitle_(locale, licenseType) {
    if (licenseType == OobeTypes.LicenseType.ENTERPRISE) {
      return this.i18n('oauthEnrollSuccessTitle');
    }
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18n('oauthEnrollEducationSuccessTitle');
    }
    return this.i18n('oauthEnrollKioskEnrollmentSuccessTitle');
  }


  /**
   * Return title for error enrollment screen.
   * @param {string} licenseType
   * @returns {string}
   * @private
   */
  getErrorTitle_(locale, licenseType) {
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18n('oauthEducationEnrollErrorTitle');
    }
    return this.i18n('oauthEnrollErrorTitle');
  }


  /**
   * Return title for error enrollment screen.
   * @param {string} licenseType
   * @returns {string}
   * @private
   */
  getAttributePromptTitle_(locale, licenseType) {
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18n('oauthEnrollEducationSuccessTitle');
    }
    return this.i18n('oauthEnrollScreenTitle');
  }

  /**
   *  Whether the "GENERIC CANCEL" (SKIP / ENROLL_MANUALLY ) button should be
   *  shown. It is only shown when in 'AUTOMATIC' mode OR when in
   *  manual mode without enrollment enforcement.
   *
   *  When the enrollment is manual AND forced, a 'BACK' button will be shown.
   * @param {Boolean} automatic - Whether the enrollment is automatic
   * @param {Boolean} enforced  - Whether the enrollment is enforced
   * @private
   */
  isGenericCancel_(automatic, enforced) {
    return automatic || (!automatic && !enforced);
  }

  /**
   * Retries the enrollment process after an error occurred in a previous
   * attempt. This goes to the C++ side through |chrome| first to clean up the
   * profile, so that the next attempt is performed with a clean state.
   */
  doRetry_() {
    chrome.send('oauthEnrollRetry');
  }

  /**
   *  Event handler for the 'Try again' button that is shown upon an error
   *  during ActiveDirectory join.
   */
  onAdJoinErrorRetry_() {
    this.showStep(OobeTypes.EnrollmentStep.AD_JOIN);
  }

  /*
   * Whether authFlow is the SAML.
   */
  isSaml_(authFlow) {
    return authFlow === AuthFlow.SAML;
  }

  /*
   * Called when we cancel TPM check early.
   */
  onTPMCheckCanceled_() {
    this.userActed('cancel-tpm-check');
  }

  // Skip enrollment dialogue section.

  /**
   * Return title for skip enrollment dialogue.
   * @param {string} licenseType
   * @returns {string}
   * @private
   */
  getSkipConfirmationTitle_(locale, licenseType) {
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18n('skipConfirmationDialogEducationTitle');
    }
    return this.i18n('skipConfirmationDialogTitle');
  }

  /**
   * Return text for skip enrollment dialogue.
   * @param {string} licenseType
   * @returns {string}
   * @private
   */
  getSkipConfirmationText_(locale, licenseType) {
    if (licenseType == OobeTypes.LicenseType.EDUCATION) {
      return this.i18n('skipConfirmationDialogEducationText');
    }
    return this.i18n('skipConfirmationDialogText');
  }

  /*
   * Called when we click go back button.
   */
  onDialogClosed_() {
    this.$.skipConfirmationDialog.hideDialog();
  }

  /*
   * Called when we click skip button.
   */
  onDialogSkip_() {
    this.$.skipConfirmationDialog.hideDialog();
    this.userActed('skip-confirmation');
  }

  showSkipConfirmationDialog() {
    this.$.skipConfirmationDialog.showDialog();
  }
}

customElements.define(
    EnterpriseEnrollmentElement.is, EnterpriseEnrollmentElement);
