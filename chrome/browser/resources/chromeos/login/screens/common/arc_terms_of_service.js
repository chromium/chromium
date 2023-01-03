// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design for ARC Terms Of
 * Service screen.
 */


import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-styles/color.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeTextButton} from '../../components/buttons/oobe_text_button.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OobeModalDialog} from '../../components/dialogs/oobe_modal_dialog.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';
import {getSelectedValue} from '../../components/oobe_select.js';
import {OobeTypes} from '../../components/oobe_types.js';
import {ContentType, WebViewHelper} from '../../components/web_view_helper.js';
import {CLEAR_ANCHORS_CONTENT_SCRIPT, WebViewLoader} from '../../components/web_view_loader.js';
import {Oobe} from '../../cr_ui.js';


// Enum that describes the current state of the Arc Terms Of Service screen
const ArcTosState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  ERROR: 'error',
};

/**
 * Timeout to load online ToS.
 * @type {number}
 */
const ONLINE_LOAD_TIMEOUT_IN_MS = 10000;

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const ArcTermsOfserviceBase = mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior, LoginScreenBehavior], PolymerElement);

/**
 * @typedef {{
 *   arcBackupRestoreChildPopup: OobeModalDialog,
 *   arcBackupRestorePopup: OobeModalDialog,
 *   arcLocationServicePopup: OobeModalDialog,
 *   arcMetricsPopup: OobeModalDialog,
 *   arcTosAcceptButton: OobeTextButton,
 *   arcTosDialog: OobeAdaptiveDialog,
 *   arcTosNextButton: OobeTextButton,
 *   arcTosOverlayPrivacyPolicy: OobeModalDialog,
 *   arcTosOverlayWebview: WebView,
 *   arcTosRetryButton: OobeTextButton,
 *   arcTosView: WebView,
 *   arcPaiPopup: OobeModalDialog,
 * }}
 */
ArcTermsOfserviceBase.$;

/**
 * @polymer
 */
class ArcTermsOfService extends ArcTermsOfserviceBase {
  static get is() {
    return 'arc-tos-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Accept, Skip and Retry buttons are disabled until content is loaded.
       */
      arcTosButtonsDisabled: {
        type: Boolean,
        value: true,
        observer: 'buttonsDisabledStateChanged_',
      },

      /**
       * Indicates whether metrics text should be hidden.
       */
      isMetricsHidden: {
        type: Boolean,
        value: false,
      },

      /**
       * String id for metrics collection text.
       */
      metricsTextKey: {
        type: String,
        value: 'arcTextMetricsEnabled',
      },

      /**
       * String id of Google service confirmation text.
       */
      googleServiceConfirmationTextKey: {
        type: String,
        value: 'arcTextGoogleServiceConfirmation',
      },

      /**
       * String id of text for Accept button.
       */
      acceptTextKey: {
        type: String,
        value: 'arcTermsOfServiceAcceptButton',
      },

      /**
       * Indicates whether backup and restore should be enabled.
       */
      backupRestore: {
        type: Boolean,
        value: true,
      },

      /**
       * Indicates whether backup and restore is managed.
       * If backup and restore is managed, the checkbox will be disabled.
       */
      backupRestoreManaged: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether current account is child account.
       */
      isChild: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether location service should be enabled.
       */
      locationService: {
        type: Boolean,
        value: true,
      },

      /**
       * Indicates whether location service is managed.
       * If location service is managed, the checkbox will be disabled.
       */
      locationServiceManaged: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether user will review Arc settings after login.
       */
      reviewSettings: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether user sees full content of terms of service.
       */
      showFullDialog: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether currently under demo mode.
       */
      demoMode: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether popup overlay webview is loading.
       */
      overlayLoading_: {
        type: Boolean,
        value: true,
      },
    };
  }

  constructor() {
    super();
    /**
     * Flag that ensures that OOBE configuration is applied only once.
     * @private {boolean}
     */
    this.configuration_applied_ = false;

    /**
     * Flag indicating if screen was shown.
     * @private {boolean}
     */
    this.is_shown_ = false;

    /**
     * Last focused element when overlay is shown. Used to resume focus when
     * overlay is dismissed.
     * @private {Object|null}
     */
    this.lastFocusedElement_ = null;

    this.countryCode_ = null;
    this.language_ = null;
    this.pageReady_ = false;

    /**
     * The hostname of the url where the terms of service will be fetched.
     * Overwritten by tests to load terms of service from local test server.
     */
    this.termsOfServiceHostName_ = 'https://play.google.com';

    this.termsError = false;
    this.usingOfflineTermsForTesting_ = false;
    this.tosContent_ = '';
    this.reloadsLeftForTesting_ = undefined;
  }

  get EXTERNAL_API() {
    return [
      'setMetricsMode',
      'setBackupAndRestoreMode',
      'setLocationServicesMode',
      'loadPlayStoreToS',
      'setArcManaged',
      'setupForDemoMode',
      'clearDemoMode',
      'setTosForTesting',
    ];
  }

  defaultUIStep() {
    return ArcTosState.LOADING;
  }

  get UI_STEPS() {
    return ArcTosState;
  }

  /** @override */
  ready() {
    super.ready();
    this.initializeLoginScreen('ArcTermsOfServiceScreen');

    if (loadTimeData.valueExists('arcTosHostNameForTesting')) {
      this.setTosHostNameForTesting_(
          loadTimeData.getString('arcTosHostNameForTesting'));
    }
  }

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.ONBOARDING;
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   */
  onBeforeShow() {
    this.is_shown_ = true;
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);

    var isDemoModeSetup = this.isDemoModeSetup_();
    if (isDemoModeSetup) {
      this.setMetricsMode('arcTextMetricsManagedEnabled', true);
    }
    this.acceptTextKey = isDemoModeSetup ?
        'arcTermsOfServiceAcceptAndContinueButton' :
        'arcTermsOfServiceAcceptButton';
    this.googleServiceConfirmationTextKey = isDemoModeSetup ?
        'arcAcceptAndContinueGoogleServiceConfirmation' :
        'arcTextGoogleServiceConfirmation';
  }

  /**
   * Called when dialog is shown for the first time.
   *
   * @private
   */
  applyOobeConfiguration_() {
    if (this.configuration_applied_) {
      return;
    }
    var configuration = Oobe.getInstance().getOobeConfiguration();
    if (!configuration) {
      return;
    }
    if (this.arcTosButtonsDisabled) {
      return;
    }
    if (configuration.arcTosAutoAccept) {
      this.onAccept_();
    }
    this.configuration_applied_ = true;
  }

  /**
   * Called whenever buttons state is updated.
   *
   * @private
   */
  buttonsDisabledStateChanged_(newValue, oldValue) {
    // Trigger applyOobeConfiguration_ if buttons are enabled and dialog is
    // visible.
    if (this.arcTosButtonsDisabled) {
      return;
    }
    if (!this.is_shown_) {
      return;
    }
    window.setTimeout(this.applyOobeConfiguration_.bind(this), 0);
  }

  /** Called when dialog is hidden. */
  onBeforeHide() {
    this.reset_();
    this.is_shown_ = false;
  }

  /**
   * Resets UI elements to their initial state.
   * @private
   */
  reset_() {
    this.showFullDialog = false;
    this.$.arcTosNextButton.focus();
  }

  /**
   * Makes sure that UI is initialized.
   *
   * @private
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  ensureInitialized_() {
    if (this.pageReady_) {
      return;
    }

    this.pageReady_ = true;

    var termsView = this.$.arcTosView;

    // Open links from webview in overlay dialog.
    var self = this;
    termsView.addEventListener('newwindow', function(event) {
      event.preventDefault();
      self.showUrlOverlay(event.targetUrl);
    });

    termsView.addContentScripts([{
      name: 'postProcess',
      matches: [this.getTermsOfServiceHostNameForMatchPattern_() + '/*'],
      css: {files: ['arc_support/playstore.css']},
      js: {files: ['arc_support/playstore.js']},
      run_at: 'document_end',
    }]);

    var overlayUrl = this.$.arcTosOverlayWebview;
    overlayUrl.addContentScripts([{
      name: 'postProcess',
      matches: ['https://support.google.com/*'],
      css: {files: ['arc_support/overlay.css']},
      run_at: 'document_end',
    }]);
  }

  /**
   * Opens external URL in popup overlay.
   * @param {string} targetUrl to show in overlay webview.
   */
  showUrlOverlay(targetUrl) {
    if (this.usingOfflineTermsForTesting_) {
      const TERMS_URL = 'chrome://terms/arc/privacy_policy';
      WebViewHelper.loadUrlContentToWebView(
          this.$.arcTosOverlayWebview, TERMS_URL,
          ContentType.PDF);
    } else {
      const overlayWebview = this.$.arcTosOverlayWebview;
      if (this.isDemoModeSetup_()) {
        this.setClearAnchorScriptForWebview_(overlayWebview);
      }
      overlayWebview.src = targetUrl;
    }

    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    this.overlayLoading_ = true;
    this.$.arcTosOverlayPrivacyPolicy.showDialog();
  }

  /**
   * Returns current language that can be updated in OOBE flow. If OOBE flow
   * does not exist then use navigator.language.
   *
   * @private
   */
  getCurrentLanguage_() {
    const LANGUAGE_LIST_ID = 'languageList';
    if (loadTimeData.valueExists(LANGUAGE_LIST_ID)) {
      var languageList = /** @type {!Array<OobeTypes.LanguageDsc>} */ (
          loadTimeData.getValue(LANGUAGE_LIST_ID));
      if (languageList) {
        var language = getSelectedValue(languageList);
        if (language) {
          return language;
        }
      }
    }
    return navigator.language;
  }

  /**
   * Sets current metrics mode.
   * @param {string} textKey Key of the text which describes current metrics
   *     state.
   * @param {boolean} visible If metrics text is visible.
   */
  setMetricsMode(textKey, visible) {
    this.isMetricsHidden = !visible;
    this.metricsTextKey = textKey;
  }

  /**
   * Sets current backup and restore mode.
   * @param {boolean} enabled Defines the value for backup and restore
   *                          checkbox.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  setBackupAndRestoreMode(enabled, managed) {
    this.backupRestore = enabled;
    this.backupRestoreManaged = managed;
  }

  /**
   * Sets current usage of location service opt in mode.
   * @param {boolean} enabled Defines the value for location service opt in.
   * @param {boolean} managed Defines whether this setting is set by policy.
   */
  setLocationServicesMode(enabled, managed) {
    this.locationService = enabled;
    this.locationServiceManaged = managed;
  }

  /**
   * Loads Play Store ToS in case country code has been changed or previous
   * attempt failed.
   * @param {string} countryCode Country code based on current timezone.
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  loadPlayStoreToS(countryCode) {
    // Make sure page is initialized for login mode. For OOBE mode, page is
    // initialized as result of handling updateLocalizedContent.
    this.ensureInitialized_();

    var language = this.getCurrentLanguage_();
    countryCode = countryCode.toLowerCase();

    if (this.language_ && this.language_ == language && this.countryCode_ &&
        this.countryCode_ == countryCode && this.uiStep != ArcTosState.ERROR &&
        !this.usingOfflineTermsForTesting_ && this.tosContent_) {
      this.enableButtons_(true);
      return;
    }

    // Store current ToS parameters.
    this.language_ = language;
    this.countryCode_ = countryCode;

    var scriptSetParameters = 'document.countryCode = \'' + countryCode + '\';';
    scriptSetParameters += 'document.language = \'' + language + '\';';
    scriptSetParameters += 'document.viewMode = \'large-view\';';

    var termsView = this.$.arcTosView;

    termsView.removeContentScripts(['preProcess']);
    termsView.addContentScripts([{
      name: 'preProcess',
      matches: [this.getTermsOfServiceHostNameForMatchPattern_() + '/*'],
      js: {code: scriptSetParameters},
      run_at: 'document_start',
    }]);

    // Try to use currently loaded document first.
    var self = this;
    if (termsView.src != '' && this.uiStep == ArcTosState.LOADED) {
      var navigateScript = 'processLangZoneTerms(true, \'' + language +
          '\', \'' + countryCode + '\');';
      termsView.executeScript({code: navigateScript}, function(results) {
        if (!results || results.length != 1 ||
            typeof results[0] !== 'boolean' || !results[0]) {
          self.reloadPlayStoreToS();
        }
      });
    } else {
      if (this.isDemoModeSetup_()) {
        this.setClearAnchorScriptForWebview_(termsView);
      }
      this.reloadPlayStoreToS();
    }
  }

  /**
   * Sets Play Store terms of service for testing.
   * @param {string} terms Fake Play Store terms of service.
   */
  setTosForTesting(terms) {
    this.tosContent_ = terms;
    this.usingOfflineTermsForTesting_ = true;
    this.setTermsViewContentLoadedState_();
  }

  /**
   * Sets Play Store hostname url used to fetch terms of service for testing.
   * @param {string} hostname hostname used to fetch terms of service.
   * @suppress {missingProperties} as WebView type has no addContentScripts
   */
  setTosHostNameForTesting_(hostname) {
    this.termsOfServiceHostName_ = hostname;
    this.reloadsLeftForTesting_ = 1;

    // Enable loading content script 'arc_support/playstore.js' when fetching
    // ToS from the test server.
    var termsView = this.$.arcTosView;
    termsView.removeContentScripts(['postProcess']);
    termsView.addContentScripts([{
      name: 'postProcess',
      matches: [this.getTermsOfServiceHostNameForMatchPattern_() + '/*'],
      css: {files: ['arc_support/playstore.css']},
      js: {files: ['arc_support/playstore.js']},
      run_at: 'document_end',
    }]);
  }

  /**
   * Sets if Arc is managed. ToS webview should not be visible if Arc is
   * manged.
   * @param {boolean} managed Defines whether this setting is set by policy.
   * @param {boolean} child whether current account is a child account.
   */
  setArcManaged(managed, child) {
    this.$.arcTosView.hidden = managed;
    this.isChild = child;
  }

  /**
   * On-tap event handler for Accept button.
   *
   * @private
   */
  onAccept_() {
    this.userActed('accept');

    this.enableButtons_(false);
    chrome.send('arcTermsOfServiceAccept', [
      this.backupRestore,
      this.locationService,
      this.reviewSettings,
      this.tosContent_,
    ]);
  }

  /**
   * Enables/Disables set of buttons: Accept, Skip, Retry.
   * @param {boolean} enable Buttons are enabled if set to true.
   *
   * @private
   */
  enableButtons_(enable) {
    this.arcTosButtonsDisabled = !enable;
  }

  /**
   * Reloads Play Store ToS.
   */
  reloadPlayStoreToS() {
    if (this.reloadsLeftForTesting_ !== undefined) {
      if (this.reloadsLeftForTesting_ <= 0) {
        return;
      }
      --this.reloadsLeftForTesting_;
    }
    this.termsError = false;
    this.usingOfflineTermsForTesting_ = false;

    const loadFailureCallback = () => {
      // If in demo mode fallback to offline Terms of Service copy.
      if (this.isDemoModeSetup_() && this.usingOfflineTermsForTesting_) {
        const TERMS_URL = 'chrome://terms/arc/terms';
        const webView = this.$.arcTosView;
        WebViewHelper.loadUrlContentToWebView(
            webView, TERMS_URL, ContentType.HTML);
        return;
      }
      this.showError_();
    };

    const termsView = this.$.arcTosView;
    const tosLoader = new WebViewLoader(
        termsView, ONLINE_LOAD_TIMEOUT_IN_MS, loadFailureCallback,
        this.isDemoModeSetup_() /* clear_anchors */, false /* inject_css */);

    const tosUrl = this.termsOfServiceHostName_ + '/about/play-terms.html';
    tosLoader.setUrl(tosUrl);

    this.setUIStep(ArcTosState.LOADING);
    this.enableButtons_(false);
  }

  /**
   * Sets up the variant of the screen dedicated falsedemo mode.
   */
  setupForDemoMode() {
    this.demoMode = true;
  }

  /**
   * Sets up the variant of the screen dedicated for demo mode.
   */
  clearDemoMode() {
    this.demoMode = false;
  }

  /**
   * Returns a match pattern compatible version of termsOfServiceHostName_ by
   * stripping the port number part of the hostname. During tests
   * termsOfServiceHostName_ will contain a port number part.
   * @return {string}
   * @private
   */
  getTermsOfServiceHostNameForMatchPattern_() {
    return this.termsOfServiceHostName_.replace(/:[0-9]+/, '');
  }

  /**
   * Handles event when terms view is loaded.
   * @suppress {missingProperties}
   */
  onTermsViewContentLoad_() {
    if (this.termsError) {
      return;
    }

    var termsView = this.$.arcTosView;

    if (this.usingOfflineTermsForTesting_) {
      // Process offline ToS for testing. Scripts added to web view by
      // addContentScripts() are not executed when using data url.
      this.tosContent_ = termsView.src;
      var setParameters =
          `document.body.classList.add('large-view', 'offline-terms');`;
      termsView.executeScript({code: setParameters}, () => {
        if (chrome.runtime.lastError) {
          console.error(
              'Set parameteters failed: ' + chrome.runtime.lastError.message);
        }
      });
      termsView.insertCSS({file: 'arc_support/playstore.css'});
      this.setTermsViewContentLoadedState_();
    } else {
      // Process online ToS.
      var getToSContent = {code: 'getToSContent();'};
      termsView.executeScript(getToSContent, this.onGetToSContent_.bind(this));
    }
  }

  /** Setups overlay webview loading callback */
  onAcrTosOverlayContentLoad_() {
    this.overlayLoading_ = false;
  }

  /**
   * Handles callback for getToSContent.
   */
  onGetToSContent_(results) {
    if (!results || results.length != 1 || typeof results[0] !== 'string') {
      this.showError_();
      return;
    }

    this.tosContent_ = results[0];
    this.setTermsViewContentLoadedState_();
  }

  /**
   * Sets the screen in the loaded state. Should be called after arc terms
   * were loaded.
   * @private
   */
  setTermsViewContentLoadedState_() {
    if (this.uiStep == ArcTosState.LOADED) {
      return;
    }
    this.setUIStep(ArcTosState.LOADED);
    this.enableButtons_(true);
    this.showFullDialog = false;
    if (this.is_shown_) {
      this.$.arcTosNextButton.focus();
    }
  }

  /**
   * Shows error UI when terms view cannot be loaded or terms content cannot
   * be fetched from webview.
   */
  showError_() {
    this.termsError = true;
    this.setUIStep(ArcTosState.ERROR);

    this.enableButtons_(true);
    this.$.arcTosRetryButton.focus();
  }

  /**
   * Updates localized content of the screen that is not updated via template.
   */
  updateLocalizedContent() {
    this.ensureInitialized_();

    // We might need to reload Play Store ToS in case language was changed.
    if (this.countryCode_) {
      this.loadPlayStoreToS(this.countryCode_);
    }
  }

  /**
   * Returns whether arc terms are shown as a part of demo mode setup.
   * @return {boolean}
   * @private
   */
  isDemoModeSetup_() {
    return this.demoMode;
  }

  onPolicyLinkClick_() {
    this.userActed('policy-link');

    var termsView = this.$.arcTosView;
    var self = this;
    termsView.executeScript(
        {code: 'getPrivacyPolicyLink();'}, function(results) {
          if (results && results.length == 1 && typeof results[0] == 'string') {
            self.showUrlOverlay(results[0]);
          } else {
            var defaultLink = 'https://www.google.com/intl/' +
                self.getCurrentLanguage_() + '/policies/privacy/';
            self.showUrlOverlay(defaultLink);
          }
        });
  }

  /**
   * On-tap event handler for Next button.
   *
   * @private
   */
  onNext_() {
    this.userActed('next');

    this.showFullDialog = true;
    this.$.arcTosDialog.scrollToBottom();
    this.$.arcTosAcceptButton.focus();
  }

  /**
   * On-tap event handler for Retry button.
   *
   * @private
   */
  onRetry_() {
    this.userActed('retry');
    this.reloadPlayStoreToS();
  }

  /**
   * On-tap event handler for Back button.
   *
   * @private
   */
  onBack_() {
    this.userActed('go-back');
  }

  /**
   * On-tap event handler for metrics learn more link
   * @private
   */
  onMetricsLearnMoreTap_() {
    this.userActed('metrics-learn-more');
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    this.$.arcMetricsPopup.showDialog();
  }

  /**
   * On-tap event handler for backup and restore learn more link
   * @private
   */
  onBackupRestoreLearnMoreTap_() {
    this.userActed('backup-restore-learn-more');
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    if (this.isChild) {
      this.$.arcBackupRestoreChildPopup.showDialog();
    } else {
      this.$.arcBackupRestorePopup.showDialog();
    }
  }

  /**
   * On-tap event handler for location service learn more link
   * @private
   */
  onLocationServiceLearnMoreTap_() {
    this.userActed('location-service-learn-more');
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    this.$.arcLocationServicePopup.showDialog();
  }

  /**
   * On-tap event handler for Play auto install learn more link
   * @private
   */
  onPaiLearnMoreTap_() {
    this.userActed('play-auto-install-learn-more');
    this.lastFocusedElement_ = this.shadowRoot.activeElement;
    this.$.arcPaiPopup.showDialog();
  }

  /*
   * Callback when overlay is closed.
   * @private
   */
  onOverlayClosed_() {
    if (this.lastFocusedElement_) {
      this.lastFocusedElement_.focus();
      this.lastFocusedElement_ = null;
    }
  }

  /**
   * Returns dialog title based on whether the active user is child.
   */
  getDialogTitle_(locale, isChild) {
    return isChild ? this.i18n('arcTermsOfServiceScreenHeadingForChild') :
                     this.i18n('arcTermsOfServiceScreenHeading');
  }

  /**
   * Set up a script for webview to clear anchor of the page after loading.
   */
  setClearAnchorScriptForWebview_(webview) {
    webview.addContentScripts([{
      name: 'clearAnchors',
      matches: ['<all_urls>'],
      js: CLEAR_ANCHORS_CONTENT_SCRIPT,
    }]);
    webview.addEventListener('contentload', () => {
      webview.executeScript(CLEAR_ANCHORS_CONTENT_SCRIPT, () => {
        if (chrome.runtime.lastError) {
          console.error(
              'Clear anchors script failed: ' +
              chrome.runtime.lastError.message);
        }
      });
    });
  }
}

customElements.define(ArcTermsOfService.is, ArcTermsOfService);
