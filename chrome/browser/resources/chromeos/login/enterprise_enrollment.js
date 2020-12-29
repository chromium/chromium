// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Enterprise Enrollment screen.
 */

(function() {

/* Code which is embedded inside of the webview. See below for details.
/** @const */
var INJECTED_WEBVIEW_SCRIPT = String.raw`
                    (function() {
                       // <include src="../keyboard/keyboard_utils.js">
                       keyboard.initializeKeyboardFlow(true);
                     })();`;

/**
 * @const
 * When making changes to any of these parameters, make sure that their use in
 * chrome/browser/resources/chromeos/login/cr_ui.js is updated as well.
 * TODO(crbug.com/1111387) - Remove this dependency when fully migrated
 * to JS modules.
 * */
var ENROLLMENT_STEP = {
  SIGNIN: 'signin',
  AD_JOIN: 'ad-join',
  WORKING: 'working',
  ATTRIBUTE_PROMPT: 'attribute-prompt',
  ERROR: 'error',
  SUCCESS: 'success',

  /* TODO(dzhioev): define this step on C++ side.
   */
  ATTRIBUTE_PROMPT_ERROR: 'attribute-prompt-error',
  ACTIVE_DIRECTORY_JOIN_ERROR: 'active-directory-join-error',
};

/**
 * The same steps as in offline-ad-login-element.
 */
const adLoginStep = {
  UNLOCK: 'unlock',
  CREDS: 'creds',
};

Polymer({
  is: 'enterprise-enrollment-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'doReload',
    'setAdJoinConfiguration',
    'setAdJoinParams',
    'setEnterpriseDomainInfo',
    'showAttributePromptStep',
    'showError',
    'showStep',
  ],

  properties: {

    /**
     * Indicates if authenticator have shown internal dialog.
     */
    authenticatorDialogDisplayed_: {
      type: Boolean,
      value: false,
    },

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
     * Whether the SAML SSO page is visible.
     * @private
     */
    isSamlSsoVisible_: {
      type: Boolean,
      value: false,
    },
  },

  defaultUIStep() {
    return ENROLLMENT_STEP.SIGNIN;
  },

  UI_STEPS: ENROLLMENT_STEP,

  /**
   * Authenticator object that wraps GAIA webview.
   */
  authenticator_: null,

  /**
   * We block esc, back button and cancel button until gaia is loaded to
   * prevent multiple cancel events.
   */
  isCancelDisabled_: null,

  get isCancelDisabled() {
    return this.isCancelDisabled_;
  },
  set isCancelDisabled(disabled) {
    this.isCancelDisabled_ = disabled;
  },

  isManualEnrollment_: undefined,

  /**
   * Value contained in the last received 'backButton' event.
   * @type {boolean}
   * @private
   */
  lastBackMessageValue_: false,

  ready() {
    this.initializeLoginScreen('OAuthEnrollmentScreen', {
      resetAllowed: true,
    });

    let authView = this.$.authView;
    this.authenticator_ = new cr.login.Authenticator(authView);

    // Establish an initial messaging between content script and
    // host script so that content script can message back.
    authView.addEventListener('loadstop', function(e) {
      e.target.contentWindow.postMessage('initialMessage', authView.src);
    });

    // When we get the advancing focus command message from injected content
    // script, we can execute it on host script context.
    window.addEventListener('message', function(e) {
      if (e.data == 'forwardFocus')
        keyboard.onAdvanceFocus(false);
      else if (e.data == 'backwardFocus')
        keyboard.onAdvanceFocus(true);
    });

    this.authenticator_.addEventListener(
        'ready', (function() {
                   if (this.uiStep != ENROLLMENT_STEP.SIGNIN)
                     return;
                   this.isCancelDisabled = false;
                   chrome.send('frameLoadingCompleted');
                 }).bind(this));

    this.authenticator_.addEventListener(
        'authCompleted',
        (function(e) {
          var detail = e.detail;
          if (!detail.email) {
            this.showError(
                loadTimeData.getString('fatalEnrollmentError'), false);
            return;
          }
          chrome.send('oauthEnrollCompleteLogin', [detail.email]);
        }).bind(this));

    this.$.adJoinUI.addEventListener('authCompleted', function(e) {
      this.$.adJoinUI.disabled = true;
      this.$.adJoinUI.loading = true;
      chrome.send('oauthEnrollAdCompleteLogin', [
        e.detail.machine_name, e.detail.distinguished_name,
        e.detail.encryption_types, e.detail.username, e.detail.password
      ]);
    }.bind(this));
    this.$.adJoinUI.addEventListener('unlockPasswordEntered', function(e) {
      this.$.adJoinUI.disabled = true;
      chrome.send(
          'oauthEnrollAdUnlockConfiguration', [e.detail.unlock_password]);
    }.bind(this));

    this.authenticator_.addEventListener(
        'authFlowChange', (function(e) {
                            var isSAML = this.authenticator_.authFlow ==
                                cr.login.Authenticator.AuthFlow.SAML;
                            if (isSAML) {
                              this.$.samlNoticeMessage.textContent =
                                  loadTimeData.getStringF(
                                      'samlNotice',
                                      this.authenticator_.authDomain);
                            }
                            this.isSamlSsoVisible_ = isSAML;
                            if (Oobe.getInstance().currentScreen == this)
                              Oobe.getInstance().updateScreenSize(this);
                            this.lastBackMessageValue_ = false;
                          }).bind(this));

    this.authenticator_.addEventListener(
        'backButton', (function(e) {
                        this.lastBackMessageValue_ = !!e.detail;
                        this.$.authView.focus();
                      }).bind(this));

    this.authenticator_.addEventListener(
        'dialogShown', (function(e) {
                         this.authenticatorDialogDisplayed_ = true;
                       }).bind(this));

    this.authenticator_.addEventListener(
        'dialogHidden', (function(e) {
                          this.authenticatorDialogDisplayed_ = false;
                        }).bind(this));

    this.authenticator_.insecureContentBlockedCallback =
        (function(url) {
          this.showError(
              loadTimeData.getStringF('insecureURLEnrollmentError', url),
              false);
        }).bind(this);

    this.authenticator_.missingGaiaInfoCallback =
        (function() {
          this.showError(
              loadTimeData.getString('fatalEnrollmentError'), false);
        }).bind(this);
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload, contains the signin frame
   * URL.
   */
  onBeforeShow(data) {
    if (Oobe.getInstance().forceKeyboardFlow) {
      // We run the tab remapping logic inside of the webview so that the
      // simulated tab events will use the webview tab-stops. Simulated tab
      // events created from the webui treat the entire webview as one tab
      // stop. Real tab events do not do this. See crbug.com/543865.
      this.$.authView.addContentScripts([{
        name: 'injectedTabHandler',
        matches: ['http://*/*', 'https://*/*'],
        js: {code: INJECTED_WEBVIEW_SCRIPT},
        run_at: 'document_start'
      }]);
    }

    this.authenticator_.setWebviewPartition(data.webviewPartitionName);

    this.isSamlSsoVisible_ = false;

    var gaiaParams = {};
    gaiaParams.gaiaUrl = data.gaiaUrl;
    gaiaParams.clientId = data.clientId;
    gaiaParams.needPassword = false;
    gaiaParams.hl = data.hl;
    if (data.management_domain) {
      gaiaParams.enterpriseEnrollmentDomain = data.management_domain;
      gaiaParams.emailDomain = data.management_domain;
    }
    gaiaParams.flow = data.flow;
    this.authenticator_.load(
        cr.login.Authenticator.AuthMode.DEFAULT, gaiaParams);

    this.isManualEnrollment_ = data.enrollment_mode === 'manual';
    this.isForced_ = data.is_enrollment_enforced;
    this.isAutoEnroll_ = data.attestationBased;

    this.authenticatorDialogDisplayed_ = false;
    cr.ui.login.invokePolymerMethod(this.$.adJoinUI, 'onBeforeShow');
    if (!this.uiStep) {
      this.showStep(data.attestationBased ?
          ENROLLMENT_STEP.WORKING : ENROLLMENT_STEP.SIGNIN);
    }
  },

  /**
   * Initial UI State for screen
   */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ENROLLMENT;
  },


  /*
   * Executed on language change.
   */
  updateLocalizedContent: function() {
    this.$.adJoinUI.i18nUpdateLocale();
    this.i18nUpdateLocale();
  },

  /**
   * Shows attribute-prompt step with pre-filled asset ID and
   * location.
   */
  showAttributePromptStep(annotatedAssetId, annotatedLocation) {
    this.assetId_ = annotatedAssetId;
    this.deviceLocation_ = annotatedLocation;
    this.showStep(ENROLLMENT_STEP.ATTRIBUTE_PROMPT);
  },


  /**
   * Sets the type of the device and the enterprise domain to be shown.
   *
   * @param {string} manager
   * @param {string} device_type
   */
  setEnterpriseDomainInfo(manager, device_type) {
    this.domainManager_ = manager;
    this.deviceName_ = device_type;
  },

  /**
   * Cancels the current authentication and drops the user back to the next
   * screen (either the next authentication or the login screen).
   */
  cancel() {
    if (this.isCancelDisabled)
      return;
    this.isCancelDisabled = true;
    this.closeEnrollment_('cancel');
  },

  /**
   * Switches between the different steps in the enrollment flow.
   * @param {string} step the steps to show, one of "signin", "working",
   * "attribute-prompt", "error", "success".
   */
  showStep(step) {
    this.setUIStep(step);
    if (step === ENROLLMENT_STEP.AD_JOIN) {
      this.$.adJoinUI.disabled = false;
      this.$.adJoinUI.loading = false;
      this.$.adJoinUI.focus();
    }
    this.isCancelDisabled =
        (step === ENROLLMENT_STEP.SIGNIN && !this.isManualEnrollment_) ||
        step === ENROLLMENT_STEP.AD_JOIN || step === ENROLLMENT_STEP.WORKING;
    this.lastBackMessageValue_ = false;
  },

  doReload() {
    this.lastBackMessageValue_ = false;
    this.authenticatorDialogDisplayed_ = false;
    this.authenticator_.reload();
  },

  /**
   * Sets Active Directory join screen params.
   * @param {string} machineName
   * @param {string} userName
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
   * @param {boolean} showUnlockConfig true if there is an encrypted
   * configuration (and not unlocked yet).
   */
  setAdJoinParams(machineName, userName, errorState, showUnlockConfig) {
    this.$.adJoinUI.disabled = false;
    this.$.adJoinUI.machineName = machineName;
    this.$.adJoinUI.userName = userName;
    this.$.adJoinUI.errorState = errorState;
    if (showUnlockConfig) {
      this.$.adJoinUI.setUIStep(adLoginStep.UNLOCK);
    } else {
      this.$.adJoinUI.setUIStep(adLoginStep.CREDS);
    }
  },

  /**
   * Sets Active Directory join screen with the unlocked configuration.
   * @param {Array<JoinConfigType>} options
   */
  setAdJoinConfiguration(options) {
    this.$.adJoinUI.disabled = false;
    this.$.adJoinUI.setJoinConfigurationOptions(options);
    this.$.adJoinUI.setUIStep(adLoginStep.CREDS);
    this.$.adJoinUI.focus();
  },

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  skipAttributes_() {
    this.showStep(ENROLLMENT_STEP.SUCCESS);
  },

  /**
   * Uploads the device attributes to server. This goes to C++ side through
   * |chrome| and launches the device attribute update negotiation.
   */
  submitAttributes_() {
    chrome.send('oauthEnrollAttributes', [this.assetId_, this.deviceLocation_]);
  },

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  onBackButtonClicked_() {
    if (this.uiStep === ENROLLMENT_STEP.SIGNIN) {
      if (this.lastBackMessageValue_) {
        this.lastBackMessageValue_ = false;
        this.$.authView.back();
      } else {
        this.cancel();
      }
    }
  },

  /**
   * Shows the learn more dialog.
   */
  onLearnMore_() {
    chrome.send('oauthEnrollOnLearnMore');
  },

  closeEnrollment_(result) {
    chrome.send('oauthEnrollClose', [result]);
  },

  /**
   * Notifies chrome that enrollment have finished.
   */
  onEnrollmentFinished_() {
    this.closeEnrollment_('done');
  },

  /**
   * Generates message on the success screen.
   */
  successText_(locale, device, domain) {
    return this.i18nAdvanced(
        'oauthEnrollAbeSuccessDomain', {substitutions: [device, domain]});
  },

  isEmpty_(str) {
    return !str;
  },

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
  showError: function(message, retry) {
    this.errorText_ = message;
    this.canRetryAfterError_ = retry;

    if (this.uiStep === ENROLLMENT_STEP.ATTRIBUTE_PROMPT) {
      this.showStep(ENROLLMENT_STEP.ATTRIBUTE_PROMPT_ERROR);
    } else if (this.uiStep === ENROLLMENT_STEP.AD_JOIN) {
      this.showStep(ENROLLMENT_STEP.ACTIVE_DIRECTORY_JOIN_ERROR);
    } else {
      this.showStep(ENROLLMENT_STEP.ERROR);
    }
  },

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
  },

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
  },

  /**
   * Retries the enrollment process after an error occurred in a previous
   * attempt. This goes to the C++ side through |chrome| first to clean up the
   * profile, so that the next attempt is performed with a clean state.
   */
  doRetry_() {
    chrome.send('oauthEnrollRetry');
  },

  /**
   *  Event handler for the 'Try again' button that is shown upon an error
   *  during ActiveDirectory join.
   */
  onAdJoinErrorRetry_() {
    this.showStep(ENROLLMENT_STEP.AD_JOIN);
  },

  /**
   * Whether to show popup overlay under the dialog
   * @param {boolean} authenticatorDialogDisplayed
   * @param {boolean} isSamlSsoVisible
   * @return {boolean} True iff overlay popup should be displayed
   * @private
   */
  showPopupOverlay_(authenticatorDialogDisplayed, isSamlSsoVisible) {
    return authenticatorDialogDisplayed || isSamlSsoVisible;
  },
});
})();
