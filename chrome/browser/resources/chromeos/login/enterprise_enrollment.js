// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for Enterprise Enrollment screen.
 */

/* Code which is embedded inside of the webview. See below for details.
/** @const */
var INJECTED_WEBVIEW_SCRIPT = String.raw`
                    (function() {
                       // <include src="../keyboard/keyboard_utils.js">
                       keyboard.initializeKeyboardFlow(true);
                     })();`;

/** @const */ var ENROLLMENT_STEP = {
  SIGNIN: 'signin',
  AD_JOIN: 'ad-join',
  LICENSE_TYPE: 'license',
  WORKING: 'working',
  ATTRIBUTE_PROMPT: 'attribute-prompt',
  ERROR: 'error',
  SUCCESS: 'success',

  /* TODO(dzhioev): define this step on C++ side.
   */
  ATTRIBUTE_PROMPT_ERROR: 'attribute-prompt-error',
  ACTIVE_DIRECTORY_JOIN_ERROR: 'active-directory-join-error',
};

Polymer({
  is: 'enterprise-enrollment',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Reference to OOBE screen object.
     * @type {!{
     *     onAuthFrameLoaded_: function(),
     *     onAuthCompleted_: function(string),
     *     onAdCompleteLogin_: function(string, string, string, string, string),
     *     onAdUnlockConfiguration_: function(string),
     *     onLicenseTypeSelected_: function(string),
     *     closeEnrollment_: function(string),
     *     onAttributesEntered_: function(string, string),
     * }}
     */
    screen: {
      type: Object,
    },

    /**
     * The current step. This is the last value passed to showStep().
     */
    currentStep_: {
      type: String,
      value: '',
    },

    /**
     * Indicates if authenticator have shown internal dialog.
     */
    authenticatorDialogDisplayed_: {
      type: Boolean,
      value: false,
    },

    /**
     * Domain the device was enrolled to.
     */
    enrolledDomain_: {
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
     * Controls if there will be "retry" button on the error screen.
     */
    canRetryAfterError_: {
      type: Boolean,
      value: true,
    },
  },

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
   * An element containing UI for picking license type.
   * @type {EnrollmentLicenseCard}
   * @private
   */
  licenseUi_: undefined,

  /**
   * An element containing navigation buttons.
   */
  navigation_: undefined,

  /**
   * An element containing UI to join an AD domain.
   * @type {OfflineAdLoginElement}
   * @private
   */
  offlineAdUi_: undefined,

  /**
   * Value contained in the last received 'backButton' event.
   * @type {boolean}
   * @private
   */
  lastBackMessageValue_: false,

  ready: function() {
    this.navigation_ = this.$['oauth-enroll-navigation'];
    this.offlineAdUi_ = this.$['oauth-enroll-ad-join-ui'];
    this.licenseUi_ = this.$['oauth-enroll-license-ui'];

    let authView = this.$['oauth-enroll-auth-view'];
    this.authenticator_ = new cr.login.Authenticator(authView);

    // Establish an initial messaging between content script and
    // host script so that content script can message back.
    authView.addEventListener('loadstop', function(e) {
      e.target.contentWindow.postMessage(
          'initialMessage', authView.src);
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
                   if (this.currentStep_ != ENROLLMENT_STEP.SIGNIN)
                     return;
                   this.isCancelDisabled = false;
                   this.screen.onAuthFrameLoaded_();
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
          this.screen.onAuthCompleted_(detail.email);
        }).bind(this));

    this.offlineAdUi_.addEventListener('authCompleted', function(e) {
      this.offlineAdUi_.disabled = true;
      this.offlineAdUi_.loading = true;
      this.screen.onAdCompleteLogin_(
        e.detail.machine_name,
        e.detail.distinguished_name,
        e.detail.encryption_types,
        e.detail.username,
        e.detail.password);
    }.bind(this));
    this.offlineAdUi_.addEventListener('unlockPasswordEntered', function(e) {
      this.offlineAdUi_.disabled = true;
      this.screen.onAdUnlockConfiguration_(e.detail.unlock_password);
    }.bind(this));

    this.authenticator_.addEventListener(
        'authFlowChange', (function(e) {
                            var isSAML = this.authenticator_.authFlow ==
                                cr.login.Authenticator.AuthFlow.SAML;
                            if (isSAML) {
                              this.$['oauth-saml-notice-message'].textContent =
                                  loadTimeData.getStringF(
                                      'samlNotice',
                                      this.authenticator_.authDomain);
                            }
                            this.classList.toggle('saml', isSAML);
                            if (Oobe.getInstance().currentScreen == this)
                              Oobe.getInstance().updateScreenSize(this);
                            this.lastBackMessageValue_ = false;
                            this.updateControlsState();
                          }).bind(this));

    this.authenticator_.addEventListener(
        'backButton', (function(e) {
                        this.lastBackMessageValue_ = !!e.detail;
                        this.$['oauth-enroll-auth-view'].focus();
                        this.updateControlsState();
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

    this.$['oauth-enroll-learn-more-link']
        .addEventListener('click', function(event) {
          chrome.send('oauthEnrollOnLearnMore');
        });


    this.licenseUi_.addEventListener('buttonclick', function() {
      this.screen.onLicenseTypeSelected_(this.licenseUi_.selected);
    }.bind(this));
  },

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param {Object} data Screen init payload, contains the signin frame
   * URL.
   */
  onBeforeShow: function(data) {
    if (Oobe.getInstance().forceKeyboardFlow) {
      // We run the tab remapping logic inside of the webview so that the
      // simulated tab events will use the webview tab-stops. Simulated tab
      // events created from the webui treat the entire webview as one tab
      // stop. Real tab events do not do this. See crbug.com/543865.
      this.$['oauth-enroll-auth-view'].addContentScripts([{
        name: 'injectedTabHandler',
        matches: ['http://*/*', 'https://*/*'],
        js: {code: INJECTED_WEBVIEW_SCRIPT},
        run_at: 'document_start'
      }]);
    }

    this.authenticator_.setWebviewPartition(data.webviewPartitionName);

    Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.ENROLLMENT);
    this.classList.remove('saml');

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

    var modes = ['manual', 'forced', 'recovery'];
    for (var i = 0; i < modes.length; ++i) {
      this.classList.toggle(
          'mode-' + modes[i], data.enrollment_mode == modes[i]);
    }
    this.isManualEnrollment_ = data.enrollment_mode === 'manual';
    this.authenticatorDialogDisplayed_ = false;

    this.offlineAdUi_.onBeforeShow();
    if (!this.currentStep_) {
      this.showStep(data.attestationBased ?
          ENROLLMENT_STEP.WORKING : ENROLLMENT_STEP.SIGNIN);
    }
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
  },

  onBeforeHide: function() {
    Oobe.getInstance().setSigninUIState(SIGNIN_UI_STATE.HIDDEN);
  },

  /**
   * Shows attribute-prompt step with pre-filled asset ID and
   * location.
   */
  showAttributePromptStep: function(annotatedAssetId, annotatedLocation) {
    this.$['oauth-enroll-asset-id'].value = annotatedAssetId;
    this.$['oauth-enroll-location'].value = annotatedLocation;
    this.showStep(ENROLLMENT_STEP.ATTRIBUTE_PROMPT);
  },

  /**
   * Shows a success card for attestation-based enrollment that shows
   * which domain the device was enrolled into.
   */
  showAttestationBasedEnrollmentSuccess: function(
      device, enterpriseEnrollmentDomain) {
    this.enrolledDomain_ = enterpriseEnrollmentDomain;
    this.deviceName_ = device;
    this.showStep(ENROLLMENT_STEP.SUCCESS);
  },

  /**
   * Cancels the current authentication and drops the user back to the next
   * screen (either the next authentication or the login screen).
   */
  cancel: function() {
    if (this.isCancelDisabled)
      return;
    this.isCancelDisabled = true;
    this.screen.closeEnrollment_('cancel');
  },

  /**
   * Updates the list of available license types in license selection dialog.
   */
  setAvailableLicenseTypes: function(licenseTypes) {
    var licenses = [
      {
        id: 'perpetual',
        label: 'perpetualLicenseTypeTitle',
      },
      {
        id: 'annual',
        label: 'annualLicenseTypeTitle',
      },
      {
        id: 'kiosk',
        label: 'kioskLicenseTypeTitle',
      }
    ];
    for (var i = 0, item; item = licenses[i]; ++i) {
      if (item.id in licenseTypes) {
        item.count = parseInt(licenseTypes[item.id]);
        item.disabled = item.count == 0;
        item.hidden = false;
      } else {
        item.count = 0;
        item.disabled = true;
        item.hidden = true;
      }
    }
    this.licenseUi_.disabled = false;
    this.licenseUi_.licenses = licenses;
  },

  /**
   * Switches between the different steps in the enrollment flow.
   * @param {string} step the steps to show, one of "signin", "working",
   * "attribute-prompt", "error", "success".
   */
  showStep: function(step) {
    this.isCancelDisabled =
        (step == ENROLLMENT_STEP.SIGNIN && !this.isManualEnrollment_) ||
        step == ENROLLMENT_STEP.AD_JOIN || step == ENROLLMENT_STEP.WORKING;

    this.currentStep_ = step;

    if (this.isErrorStep_(step)) {
      this.$['oauth-enroll-error-card'].submitButton.focus();
    } else if (step == ENROLLMENT_STEP.SIGNIN) {
      this.$['oauth-enroll-auth-view'].focus();
    } else if (step == ENROLLMENT_STEP.LICENSE_TYPE) {
      this.$['oauth-enroll-license-ui'].show();
    } else if (step == ENROLLMENT_STEP.SUCCESS) {
      this.$['oauth-enroll-success-card'].show();
    } else if (step == ENROLLMENT_STEP.ATTRIBUTE_PROMPT) {
      this.$['oauth-enroll-attribute-prompt-card'].show();
    } else if (step == ENROLLMENT_STEP.AD_JOIN) {
      this.offlineAdUi_.disabled = false;
      this.offlineAdUi_.loading = false;
      this.offlineAdUi_.focus();
    }

    this.lastBackMessageValue_ = false;
    this.updateControlsState();
  },

  /**
   * Sets an error message and switches to the error screen.
   * @param {string} message the error message.
   * @param {boolean} retry whether the retry link should be shown.
   */
  showError: function(message, retry) {
    this.errorText_ = message;
    this.canRetryAfterError_ = retry;

    if (this.currentStep_ == ENROLLMENT_STEP.ATTRIBUTE_PROMPT) {
      this.showStep(ENROLLMENT_STEP.ATTRIBUTE_PROMPT_ERROR);
    } else if (this.currentStep_ == ENROLLMENT_STEP.AD_JOIN) {
      this.showStep(ENROLLMENT_STEP.ACTIVE_DIRECTORY_JOIN_ERROR);
    } else {
      this.showStep(ENROLLMENT_STEP.ERROR);
    }
  },

  doReload: function() {
    this.lastBackMessageValue_ = false;
    this.authenticatorDialogDisplayed_ = false;
    this.authenticator_.reload();
    this.updateControlsState();
  },

  /**
   * Sets Active Directory join screen params.
   * @param {string} machineName
   * @param {string} userName
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
   * @param {boolean} showUnlockConfig true if there is an encrypted
   * configuration (and not unlocked yet).
   */
  setAdJoinParams: function(
      machineName, userName, errorState, showUnlockConfig) {
    this.offlineAdUi_.disabled = false;
    this.offlineAdUi_.machineName = machineName;
    this.offlineAdUi_.userName = userName;
    this.offlineAdUi_.errorState = errorState;
    this.offlineAdUi_.unlockPasswordStep = showUnlockConfig;
  },

  /**
   * Sets Active Directory join screen with the unlocked configuration.
   * @param {Array<JoinConfigType>} options
   */
  setAdJoinConfiguration: function(options) {
    this.offlineAdUi_.disabled = false;
    this.offlineAdUi_.setJoinConfigurationOptions(options);
    this.offlineAdUi_.unlockPasswordStep = false;
  },

  /**
   * Retries the enrollment process after an error occurred in a previous
   * attempt. This goes to the C++ side through |chrome| first to clean up the
   * profile, so that the next attempt is performed with a clean state.
   */
  doRetry_: function() {
    chrome.send('oauthEnrollRetry');
  },

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  skipAttributes_: function() {
    this.showStep(ENROLLMENT_STEP.SUCCESS);
  },

  /**
   * Uploads the device attributes to server. This goes to C++ side through
   * |chrome| and launches the device attribute update negotiation.
   */
  submitAttributes_: function() {
    this.screen.onAttributesEntered_(this.$['oauth-enroll-asset-id'].value,
        this.$['oauth-enroll-location'].value);
  },

  /**
   * Skips the device attribute update,
   * shows the successful enrollment step.
   */
  onBackButtonClicked_: function() {
    if (this.currentStep_ == ENROLLMENT_STEP.SIGNIN) {
      if (this.lastBackMessageValue_) {
        this.lastBackMessageValue_ = false;
        this.$['oauth-enroll-auth-view'].back();
      } else {
        this.cancel();
      }
    }
  },

  /**
   * Returns true if we are at the begging of enrollment flow (i.e. the email
   * page).
   *
   * @type {boolean}
   */
  isAtTheBeginning: function() {
    return !this.lastBackMessageValue_ &&
        this.currentStep_ == ENROLLMENT_STEP.SIGNIN;
  },

  /**
   * Updates visibility of navigation buttons.
   */
  updateControlsState: function() {
    this.navigation_.refreshVisible = this.isAtTheBeginning() &&
        this.isManualEnrollment_ === false;
    this.navigation_.closeVisible =
        (this.currentStep_ == ENROLLMENT_STEP.ERROR
            && !this.navigation_.refreshVisible) ||
        this.currentStep_ == ENROLLMENT_STEP.LICENSE_TYPE;
  },

  /**
   * Notifies chrome that enrollment have finished.
   */
  onEnrollmentFinished_: function() {
    this.screen.closeEnrollment_('done');
  },

  updateLocalizedContent: function() {
    this.offlineAdUi_.i18nUpdateLocale();
  },

  onErrorButtonPressed_: function () {
    if (this.currentStep_ == ENROLLMENT_STEP.ACTIVE_DIRECTORY_JOIN_ERROR) {
      this.showStep(ENROLLMENT_STEP.AD_JOIN);
    } else if (this.currentStep_ == ENROLLMENT_STEP.ATTRIBUTE_PROMPT_ERROR) {
      this.onEnrollmentFinished_();
    } else {
      this.doRetry_();
    }
  },

  /**
   * Generates message on the success screen.
   */
  successText_: function(locale, device, domain) {
    return this.i18nAdvanced(
        'oauthEnrollAbeSuccessDomain', {substitutions: [device, domain]});
  },

  isEmpty_: function(str) {
    return !str;
  },

  /**
   * Simple equality comparison function.
   */
  eq_: function(currentStep, expectedStep) {
    return currentStep == expectedStep;
  },

  /**
   * Simple equality comparison function.
   */
  isErrorStep_: function(currentStep) {
    return currentStep == ENROLLMENT_STEP.ERROR ||
           currentStep == ENROLLMENT_STEP.ATTRIBUTE_PROMPT_ERROR ||
           currentStep == ENROLLMENT_STEP.ACTIVE_DIRECTORY_JOIN_ERROR;
  },

  /**
   * Text for error screen button depending on type of error.
   */
  errorAction_: function(locale, step, retry) {
    if (this.currentStep_ == ENROLLMENT_STEP.ACTIVE_DIRECTORY_JOIN_ERROR) {
      return this.i18n('oauthEnrollRetry');
    } else if (this.currentStep_ == ENROLLMENT_STEP.ATTRIBUTE_PROMPT_ERROR) {
      return this.i18n('oauthEnrollDone');
    } else if (this.currentStep_ == ENROLLMENT_STEP.ERROR) {
      return retry ? this.i18n('oauthEnrollRetry') : '';
    }
  }

});
