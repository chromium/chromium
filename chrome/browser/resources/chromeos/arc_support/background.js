// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Chrome window that hosts UI. Only one window is allowed.
 * @type {chrome.app.window.AppWindow}
 */
let appWindow = null;

/** @type {TermsOfServicePage} */
let termsPage = null;

/**
 * Used for bidirectional communication with native code.
 * @type {chrome.runtime.Port}
 */
let port = null;

/**
 * Stores current device id.
 * @type {string}
 */
let currentDeviceId = null;

/**
 * Stores last focused element before showing overlay. It is used to restore
 * focus once overlay is closed.
 * @type {Object}
 */
let lastFocusedElement = null;

/**
 * Stores locale set for the current browser process.
 * @type {string}
 */
let locale = null;

/**
 * Host window outer default width.
 * @const {number}
 */
const OUTER_WIDTH = 768;

/**
 * Host window outer default height.
 * @const {number}
 */
const OUTER_HEIGHT = 640;


/**
 * Sends a native message to ArcSupportHost.
 * @param {string} event The event type in message.
 * @param {Object=} opt_props Extra properties for the message.
 */
function sendNativeMessage(event, opt_props) {
  const message = Object.assign({'event': event}, opt_props);
  port.postMessage(message);
}

/**
 * Class to handle checkbox corresponding to a preference.
 */
class PreferenceCheckbox {
  /**
   * Creates a Checkbox which handles the corresponding preference update.
   * @param {Element} container The container this checkbox corresponds to.
   *     The element must have <input type="checkbox" class="checkbox-option">
   *     for the checkbox itself, and <p class="checkbox-text"> for its label.
   * @param {string} learnMoreContent I18n content which is shown when "Learn
   *     More" link is clicked.
   * @param {string?} learnMoreLinkId The ID for the "Learn More" link element.
   *     TODO: Get rid of this. The element can have class so that it can be
   *     identified easily. Also, it'd be better to extract the link element
   *     (tag) from the i18n text, and let i18n focus on the content.
   * @param {string?} policyText The content of the policy indicator.
   */
  constructor(container, learnMoreContent, learnMoreLinkId, policyText) {
    this.container_ = container;
    this.learnMoreContent_ = learnMoreContent;

    this.checkbox_ = container.querySelector('.checkbox-option');
    this.label_ = container.querySelector('.checkbox-text');

    this.isManaged_ = false;

    const learnMoreLink = this.label_.querySelector(learnMoreLinkId);
    if (learnMoreLink) {
      learnMoreLink.addEventListener(
          'click', (event) => this.onLearnMoreLinkClicked(event));
      learnMoreLink.addEventListener(
          'keydown', (event) => this.suppressKeyDown(event));
    }

    // Create controlled indicator for policy if necessary.
    if (policyText) {
      this.policyIndicator_ =
          new appWindow.contentWindow.cr.ui.ControlledIndicator();
      this.policyIndicator_.setAttribute('textpolicy', policyText);
      // TODO: better to have a dedicated element for this place.
      this.label_.insertBefore(this.policyIndicator_, learnMoreLink);
    } else {
      this.policyIndicator_ = null;
    }
  }

  /**
   * Returns if the checkbox is checked or not. Note that this *may* be
   * different from the preference value, because the user's check is
   * not propagated to the preference until the user clicks "AGREE" button.
   */
  isChecked() {
    return this.checkbox_.checked;
  }

  /**
   * Returns if the checkbox reflects a managed setting, rather than a
   * user-controlled setting.
   */
  isManaged() {
    return this.isManaged_;
  }

  /**
   * Called when the preference value in native code is updated.
   */
  onPreferenceChanged(isEnabled, isManaged) {
    this.checkbox_.checked = isEnabled;
    this.checkbox_.disabled = isManaged;
    this.label_.disabled = isManaged;
    this.isManaged_ = isManaged;

    if (this.policyIndicator_) {
      if (isManaged) {
        this.policyIndicator_.setAttribute('controlled-by', 'policy');
      } else {
        this.policyIndicator_.removeAttribute('controlled-by');
      }
    }
  }

  /**
   * Called when the "Learn More" link is clicked.
   */
  onLearnMoreLinkClicked(event) {
    showTextOverlay(this.learnMoreContent_);
    event.stopPropagation();
  }

  /**
   * Called when a key is pressed down on the "Learn More" or "Settings" links.
   * This prevent propagation of the current event in order to prevent parent
   * check box toggles its state.
   */
  suppressKeyDown(event) {
    event.stopPropagation();
  }
}

/**
 * Handles the checkbox action of metrics preference.
 * This has special customization e.g. show/hide the checkbox based on
 * the native preference.
 */
class MetricsPreferenceCheckbox extends PreferenceCheckbox {
  constructor(
      container, learnMoreContent, learnMoreLinkId, isOwner, textDisabled,
      textEnabled, textManagedDisabled, textManagedEnabled) {
    // Do not use policy indicator.
    // Learn More link handling is done by this class.
    // So pass |null| intentionally.
    super(container, learnMoreContent, null, null);

    this.textLabel_ = container.querySelector('.content-text');
    this.learnMoreLinkId_ = learnMoreLinkId;
    this.isOwner_ = isOwner;

    // Two dimensional array. First dimension is whether it is managed or not,
    // the second one is whether it is enabled or not.
    this.texts_ = [
      [textDisabled, textEnabled],
      [textManagedDisabled, textManagedEnabled],
    ];
  }

  onPreferenceChanged(isEnabled, isManaged) {
    isManaged = isManaged || !this.isOwner_;
    super.onPreferenceChanged(isEnabled, isManaged);

    // Hide the checkbox if it is not allowed to (re-)enable.
    // TODO(jhorwich) Remove checkbox functionality from the metrics notice as
    // we've removed the ability for a device owner to enable it during ARC
    // setup.
    const canEnable = false;
    this.checkbox_.hidden = !canEnable;
    this.textLabel_.hidden = canEnable;
    const label = canEnable ? this.label_ : this.textLabel_;

    // Update label text.
    label.innerHTML = this.texts_[isManaged ? 1 : 0][isEnabled ? 1 : 0];

    // Work around for the current translation text.
    // The translation text has tags for following links, although those
    // tags are not the target of the translation (but those content text is
    // the translation target).
    // So, meanwhile, we set the link every time we update the text.
    // TODO: fix the translation text, and main html.
    const learnMoreLink = label.querySelector(this.learnMoreLinkId_);
    if (learnMoreLink) {
      learnMoreLink.addEventListener(
          'click', (event) => this.onLearnMoreLinkClicked(event));
      learnMoreLink.addEventListener(
          'keydown', (event) => this.suppressKeyDown(event));
    }
    // settings-link is used only in privacy section.
    const settingsLink = label.querySelector('#settings-link');
    if (settingsLink) {
      settingsLink.addEventListener(
          'click', (event) => this.onPrivacySettingsLinkClicked(event));
      settingsLink.addEventListener(
          'keydown', (event) => this.suppressKeyDown(event));
    }
  }

  /** Called when "privacy settings" link is clicked. */
  onPrivacySettingsLinkClicked(event) {
    sendNativeMessage('onOpenPrivacySettingsPageClicked');
    event.stopPropagation();
  }
}

/**
 * Represents the page loading state.
 * @enum {number}
 */
const LoadState = {
  UNLOADED: 0,
  LOADING: 1,
  ABORTED: 2,
  LOADED: 3,
};

/**
 * Handles events for Terms-Of-Service page. Also this implements the async
 * loading of Terms-Of-Service content.
 */
class TermsOfServicePage {
  /**
   * @param {Element} container The container of the page.
   * @param {boolean} isManaged Set true if ARC is managed.
   * @param {string} countryCode The country code for the terms of service.
   * @param {MetricsPreferenceCheckbox} metricsCheckbox. The checkbox for the
   *     metrics preference.
   * @param {PreferenceCheckbox} backupRestoreCheckbox The checkbox for the
   *     backup-restore preference.
   * @param {PreferenceCheckbox} locationServiceCheckbox The checkbox for the
   *     location service.
   * @param {string} learnMorePaiService. Contents of learn more link of Play
   *     auto install service.
   */
  constructor(
      container, isManaged, countryCode, metricsCheckbox, backupRestoreCheckbox,
      locationServiceCheckbox, learnMorePaiService) {
    this.loadingContainer_ =
        container.querySelector('#terms-of-service-loading');
    this.contentContainer_ =
        container.querySelector('#terms-of-service-content');

    this.metricsCheckbox_ = metricsCheckbox;
    this.backupRestoreCheckbox_ = backupRestoreCheckbox;
    this.locationServiceCheckbox_ = locationServiceCheckbox;

    this.isManaged_ = isManaged;

    this.tosContent_ = '';
    this.tosShown_ = false;

    // Set event listener for webview loading.
    this.termsView_ = container.querySelector('#terms-view');
    this.termsView_.addEventListener(
        'loadstart', () => this.onTermsViewLoadStarted_());
    this.termsView_.addEventListener(
        'contentload', () => this.onTermsViewLoaded_());
    this.termsView_.addEventListener(
        'loadabort', (event) => this.onTermsViewLoadAborted_(event.reason));
    const requestFilter = {urls: ['<all_urls>'], types: ['main_frame']};
    this.termsView_.request.onCompleted.addListener(
        this.onTermsViewRequestCompleted_.bind(this), requestFilter);
    this.countryCode = countryCode.toLowerCase();

    let scriptInitTermsView =
        'document.countryCode = \'' + this.countryCode + '\';';
    scriptInitTermsView += 'document.language = \'' + locale + '\';';
    scriptInitTermsView += 'document.viewMode = \'large-view\';';
    this.termsView_.addContentScripts([
      {
        name: 'preProcess',
        matches: ['<all_urls>'],
        js: {code: scriptInitTermsView},
        run_at: 'document_start',
      },
      {
        name: 'postProcess',
        matches: ['<all_urls>'],
        css: {files: ['playstore.css']},
        js: {files: ['playstore.js']},
        run_at: 'document_end',
      },
    ]);

    // webview is not allowed to open links in the new window. Hook these
    // events and open links in overlay dialog.
    this.termsView_.addEventListener('newwindow', function(event) {
      event.preventDefault();
      showURLOverlay(event.targetUrl);
    });
    this.state_ = LoadState.UNLOADED;

    this.serviceContainer_ = container.querySelector('#service-container');
    this.locationService_ =
        container.querySelector('#location-service-preference');
    this.paiService_ = container.querySelector('#pai-service-description');
    this.googleServiceConfirmation_ =
        container.querySelector('#google-service-confirmation');
    this.agreeButton_ = container.querySelector('#button-agree');
    this.nextButton_ = container.querySelector('#button-next');

    // On managed case, do not show TermsOfService section. Note that the
    // checkbox for the preferences are still visible.
    const visibility = isManaged ? 'hidden' : 'visible';
    container.querySelector('#terms-container').style.visibility = visibility;

    // PAI service.
    const paiLabel = this.paiService_.querySelector('.content-text');
    const paiLearnMoreLink = paiLabel.querySelector('#learn-more-link-pai');
    if (paiLearnMoreLink) {
      paiLearnMoreLink.onclick = function(event) {
        event.stopPropagation();
        showTextOverlay(learnMorePaiService);
      };
    }

    // Set event handler for buttons.
    this.agreeButton_.addEventListener('click', () => this.onAgree());
    this.nextButton_.addEventListener('click', () => this.onNext_());
    container.querySelector('#button-cancel')
        .addEventListener('click', () => this.onCancel_());
  }

  /** Called when the TermsOfService page is shown. */
  onShow() {
    if (this.isManaged_ || this.state_ === LoadState.LOADED) {
      // Note: in managed case, because it does not show the contents of terms
      // of service, it is ok to show the content container immediately.
      this.showContent_();
    } else {
      this.startTermsViewLoading_();
    }
  }

  /** Shows the loaded terms-of-service content. */
  showContent_() {
    this.loadingContainer_.hidden = true;
    this.contentContainer_.hidden = false;
    this.locationService_.hidden = true;
    this.paiService_.hidden = true;
    this.googleServiceConfirmation_.hidden = true;
    this.serviceContainer_.style.overflow = 'hidden';
    this.agreeButton_.hidden = true;
    this.nextButton_.hidden = false;
    this.updateTermsHeight_();
    this.nextButton_.focus();
    if (!this.termsView_.src.startsWith('https://play.google/play-terms')) {
      // This is reload due to language selection. Set focus on dropdown to pass
      // GAR criteria(b/308537845)
      const getDropDown = {code: 'getLangZoneSelect();'};
      termsPage.termsView_.executeScript(
          getDropDown, this.focusOnLangZoneSelect_.bind(this));
    }
  }

  /** Callback for getDropDown in showContext_. */
  focusOnLangZoneSelect_(results) {
    if (results.length !== 1) {
      console.error('unexpected return value of the script');
      return;
    }
    if (results[0]) {
      this.termsView_.focus();
      const details = {code: 'getLangZoneSelect().focus();'};
      termsPage.termsView_.executeScript(details, function(results) {});
    }
  }

  onNext_() {
    this.locationService_.hidden = false;
    this.paiService_.hidden = false;
    this.googleServiceConfirmation_.hidden = false;
    this.serviceContainer_.style.overflowY = 'auto';
    this.serviceContainer_.scrollTop = this.serviceContainer_.scrollHeight;
    this.agreeButton_.hidden = false;
    this.nextButton_.hidden = true;
    this.agreeButton_.focus();
  }

  /**
   * Updates terms view height manually because webview is not automatically
   * resized in case parent div element gets resized.
   */
  updateTermsHeight_() {
    // Update the height in next cycle to prevent webview animation and
    // wrong layout caused by whole-page layout change.
    setTimeout(function() {
      const doc = appWindow.contentWindow.document;
      // Reset terms-view height in order to stabilize style computation. For
      // some reason, child webview affects final result.
      this.termsView_.style.height = '0px';
      const termsContainer =
          this.contentContainer_.querySelector('#terms-container');
      const style = window.getComputedStyle(termsContainer, null);
      this.termsView_.style.height = style.getPropertyValue('height');
    }.bind(this), 0);
  }

  /** Starts to load the terms of service webview content. */
  startTermsViewLoading_() {
    if (this.state_ === LoadState.LOADING) {
      // If there already is inflight loading task, do nothing.
      return;
    }

    const defaultLocation = 'https://play.google/play-terms/';
    if (this.termsView_.src) {
      // This is reloading the page, typically clicked RETRY on error page.
      this.fastLocation_ = undefined;
      if (this.termsView_.src === defaultLocation) {
        this.termsView_.reload();
      } else {
        this.termsView_.src = defaultLocation;
      }
    } else {
      // startTermsViewLoading used to have load time optimization logic
      // (b/62540008), but this logic was removed because ToS webpage
      // load time had improved.
      this.termsView_.src = defaultLocation;
    }
  }

  /** Returns user choices and page configuration for processing. */
  getPageResults_() {
    return {
      tosContent: this.tosContent_,
      tosShown: this.tosShown_,
      isMetricsEnabled: this.metricsCheckbox_.isChecked(),
      isBackupRestoreEnabled: this.backupRestoreCheckbox_.isChecked(),
      isBackupRestoreManaged: this.backupRestoreCheckbox_.isManaged(),
      isLocationServiceEnabled: this.locationServiceCheckbox_.isChecked(),
      isLocationServiceManaged: this.locationServiceCheckbox_.isManaged(),
    };
  }

  /** Called when the terms-view starts to be loaded. */
  onTermsViewLoadStarted_() {
    // Note: Reloading can be triggered by user action. E.g., user may select
    // their language by selection at the bottom of the Terms Of Service
    // content.
    this.state_ = LoadState.LOADING;
    this.tosContent_ = '';
    // Show loading page.
    this.loadingContainer_.hidden = false;
    this.contentContainer_.hidden = true;
  }

  /** Called when the terms-view is loaded. */
  onTermsViewLoaded_() {
    // This is called also when the loading is failed.
    // In such a case, onTermsViewLoadAborted_() is called in advance, and
    // state_ is set to ABORTED. Here, switch the view only for the
    // successful loading case.
    if (this.state_ === LoadState.LOADING) {
      const getToSContent = {code: 'getToSContent();'};
      termsPage.termsView_.executeScript(
          getToSContent, this.onGetToSContent_.bind(this));
    }
  }

  /** Callback for getToSContent. */
  onGetToSContent_(results) {
    if (this.state_ === LoadState.LOADING) {
      if (!results || results.length !== 1 || typeof results[0] !== 'string') {
        this.onTermsViewLoadAborted_('unable to get ToS content');
        return;
      }
      onTosLoadResult(true /*success*/);
      this.state_ = LoadState.LOADED;
      this.tosContent_ = results[0];
      this.tosShown_ = true;
      this.showContent_();

      if (this.fastLocation_) {
        // For fast location load make sure we have right terms displayed.
        this.fastLocation_ = undefined;
        const checkInitialLangZoneTerms = 'processLangZoneTerms(true, \'' +
            locale + '\', \'' + this.countryCode + '\');';
        const details = {code: checkInitialLangZoneTerms};
        termsPage.termsView_.executeScript(details, function(results) {});
      }
    }
  }

  /** Called when the terms-view loading is aborted. */
  onTermsViewLoadAborted_(reason) {
    console.error('TermsView loading is aborted: ' + reason);
    // Mark ABORTED so that onTermsViewLoaded_() won't show the content view.
    this.fastLocation_ = undefined;
    this.state_ = LoadState.ABORTED;
    onTosLoadResult(false /*success*/);
    showErrorPage(
        appWindow.contentWindow.loadTimeData.getString('serverError'),
        true /*opt_shouldShowSendFeedback*/,
        true /*opt_shouldShowNetworkTests*/);
  }

  /** Called when the terms-view's load request is completed. */
  onTermsViewRequestCompleted_(details) {
    if (this.state_ !== LoadState.LOADING || details.statusCode === 200) {
      return;
    }

    // In case we failed with fast location let retry default scheme.
    if (this.fastLocation_) {
      this.fastLocation_ = undefined;
      this.termsView_.src = 'https://play.google/play-terms/';
      return;
    }
    this.onTermsViewLoadAborted_(
        'request failed with status ' + details.statusCode);
  }

  /** Called when "AGREE" button is clicked. */
  onAgree() {
    sendNativeMessage('onAgreed', this.getPageResults_());
  }

  /** Called when "CANCEL" button is clicked. */
  onCancel_() {
    sendNativeMessage('onCanceled', this.getPageResults_());
    closeWindow();
  }

  /** Called when metrics preference is updated. */
  onMetricsPreferenceChanged(isEnabled, isManaged) {
    this.metricsCheckbox_.onPreferenceChanged(isEnabled, isManaged);

    // Applying metrics mode may change page layout, update terms height.
    this.updateTermsHeight_();
  }

  /** Called when backup-restore preference is updated. */
  onBackupRestorePreferenceChanged(isEnabled, isManaged) {
    this.backupRestoreCheckbox_.onPreferenceChanged(isEnabled, isManaged);
  }

  /** Called when location service preference is updated. */
  onLocationServicePreferenceChanged(isEnabled, isManaged) {
    this.locationServiceCheckbox_.onPreferenceChanged(isEnabled, isManaged);
  }
}

/**
 * Applies localization for html content and sets terms webview.
 * @param {!Object} data Localized strings and relevant information.
 * @param {string} deviceId Current device id.
 */
function initialize(data, deviceId) {
  currentDeviceId = deviceId;
  const doc = appWindow.contentWindow.document;
  const loadTimeData = appWindow.contentWindow.loadTimeData;
  loadTimeData.data = data;
  appWindow.contentWindow.i18nTemplate.process(doc, loadTimeData);
  locale = loadTimeData.getString('locale');

  // Initialize preference connected checkboxes in terms of service page.
  termsPage = new TermsOfServicePage(
      doc.getElementById('terms'), data.arcManaged, data.countryCode,
      new MetricsPreferenceCheckbox(
          doc.getElementById('metrics-preference'), data.learnMoreStatistics,
          '#learn-more-link-metrics', data.isOwnerProfile,
          data.textMetricsDisabled, data.textMetricsEnabled,
          data.textMetricsManagedDisabled, data.textMetricsManagedEnabled),
      new PreferenceCheckbox(
          doc.getElementById('backup-restore-preference'),
          data.learnMoreBackupAndRestore, '#learn-more-link-backup-restore',
          data.controlledByPolicy),
      new PreferenceCheckbox(
          doc.getElementById('location-service-preference'),
          data.learnMoreLocationServices, '#learn-more-link-location-service',
          data.controlledByPolicy),
      data.learnMorePaiService);

  doc.getElementById('close-button').title =
      loadTimeData.getString('overlayClose');

  adjustTopMargin();
}

// With UI request to change inner window size to outer window size and reduce
// top spacing, adjust top margin to negative window top bar height.
function adjustTopMargin() {
  if (!appWindow) {
    return;
  }

  const decorationHeight =
      appWindow.outerBounds.height - appWindow.innerBounds.height;

  const doc = appWindow.contentWindow.document;
  const headers = doc.getElementsByClassName('header');
  for (let i = 0; i < headers.length; i++) {
    headers[i].style.marginTop = -decorationHeight + 'px';
  }
}

/**
 * Handles native messages received from ArcSupportHost.
 * @param {!Object} message The message received.
 */
function onNativeMessage(message) {
  if (!message.action) {
    return;
  }

  if (!appWindow) {
    console.warn('Received native message when window is not available.');
    return;
  }

  if (message.action === 'initialize') {
    initialize(message.data, message.deviceId);
  } else if (message.action === 'setMetricsMode') {
    termsPage.onMetricsPreferenceChanged(message.enabled, message.managed);
  } else if (message.action === 'setBackupAndRestoreMode') {
    termsPage.onBackupRestorePreferenceChanged(
        message.enabled, message.managed);
  } else if (message.action === 'setLocationServiceMode') {
    termsPage.onLocationServicePreferenceChanged(
        message.enabled, message.managed);
  } else if (message.action === 'showPage') {
    showPage(message.page);
  } else if (message.action === 'showErrorPage') {
    showErrorPage(
        message.errorMessage, message.shouldShowSendFeedback,
        message.shouldShowNetworkTests);
  } else if (message.action === 'closeWindow') {
    closeWindow();
  } else if (message.action === 'setWindowBounds') {
    setWindowBounds(
        message.displayWorkareaX, message.displayWorkareaY,
        message.displayWorkareaWidth, message.displayWorkareaHeight);
  }
}

/**
 * Connects to ArcSupportHost.
 */
function connectPort() {
  const hostName = 'com.google.arc_support';
  port = chrome.runtime.connectNative(hostName);
  port.onMessage.addListener(onNativeMessage);
}

/**
 * Shows requested page and hide others. Show appWindow if it was hidden before.
 * 'none' hides all views.
 * @param {string} pageDivId id of divider of the page to show.
 */
function showPage(pageDivId) {
  if (!appWindow) {
    return;
  }

  hideOverlay();
  appWindow.contentWindow.stopProgressAnimation();
  const doc = appWindow.contentWindow.document;

  const pages = doc.getElementsByClassName('section');
  for (let i = 0; i < pages.length; i++) {
    pages[i].hidden = pages[i].id !== pageDivId;
  }

  appWindow.show();
  if (pageDivId === 'terms') {
    termsPage.onShow();
  }

  // Start progress bar animation for the page that has the dynamic progress
  // bar. 'error' page has the static progress bar that no need to be animated.
  if (pageDivId === 'terms' || pageDivId === 'arc-loading') {
    appWindow.contentWindow.startProgressAnimation(pageDivId);
  }
}

/**
 * Sends a message to host that TOS load has failed or succeeded.
 *
 * @param {boolean} success If set to true, loading has succeeded. False
 *     otherwise.
 */
function onTosLoadResult(success) {
  sendNativeMessage('onTosLoadResult', {success: success});
}

/**
 * Shows an error page, with given errorMessage.
 *
 * @param {string} errorMessage Localized error message text.
 * @param {?boolean} opt_shouldShowSendFeedback If set to true, show "Send
 *     feedback" button.
 * @param {?boolean} opt_shouldShowNetworkTests If set to true, show "Check
 *     network" button. If set to false, position the "Send feedback" button
 *     after the flex div that separates the left and right side of the error
 *     dialog.
 */
function showErrorPage(
    errorMessage, opt_shouldShowSendFeedback, opt_shouldShowNetworkTests) {
  if (!appWindow) {
    return;
  }

  const doc = appWindow.contentWindow.document;
  const messageElement = doc.getElementById('error-message');
  messageElement.innerText = errorMessage;

  const sendFeedbackElement = doc.getElementById('button-send-feedback');
  sendFeedbackElement.hidden = !opt_shouldShowSendFeedback;

  const networkTestsElement = doc.getElementById('button-run-network-tests');
  networkTestsElement.hidden = !opt_shouldShowNetworkTests;
  showPage('error');

  // If the error is not network-related, position send feedback after the flex
  // div.
  const feedbackSeparator = doc.getElementById('div-error-separating-buttons');
  feedbackSeparator.style.order = opt_shouldShowNetworkTests ? 'initial' : -1;

  sendNativeMessage('onErrorPageShown', {
    networkTestsShown: opt_shouldShowNetworkTests,
  });
}

/**
 * Shows overlay dialog and required content.
 * @param {string} overlayClass Defines which content to show, 'overlay-url' for
 *                              webview based content and 'overlay-text' for
 *                              simple text view.
 */
function showOverlay(overlayClass) {
  const doc = appWindow.contentWindow.document;
  const overlayContainer = doc.getElementById('overlay-container');
  overlayContainer.classList.remove('overlay-text');
  overlayContainer.classList.remove('overlay-url');
  overlayContainer.classList.add('overlay-loading');
  overlayContainer.classList.add(overlayClass);
  overlayContainer.hidden = false;
  lastFocusedElement = doc.activeElement;
  doc.getElementById('overlay-close').focus();
}

/**
 * Opens overlay dialog and shows formatted text content there.
 * @param {string} content HTML formatted text to show.
 */
function showTextOverlay(content) {
  const doc = appWindow.contentWindow.document;
  const textContent = doc.getElementById('overlay-text-content');
  textContent.innerHTML = content;
  showOverlay('overlay-text');
}

/**
 * Opens overlay dialog and shows external URL there.
 * @param {string} url Target URL to open in overlay dialog.
 */
function showURLOverlay(url) {
  const doc = appWindow.contentWindow.document;
  const overlayWebview = doc.getElementById('overlay-url');
  overlayWebview.src = url;
  showOverlay('overlay-url');
}

/**
 * Shows Google Privacy Policy in overlay dialog. Policy link is detected from
 * the content of terms view.
 */
function showPrivacyPolicyOverlay() {
  const defaultLink =
      'https://www.google.com/intl/' + locale + '/policies/privacy/';
  if (termsPage.isManaged_) {
    showURLOverlay(defaultLink);
    return;
  }
  const details = {code: 'getPrivacyPolicyLink();'};
  termsPage.termsView_.executeScript(details, function(results) {
    if (results && results.length === 1 && typeof results[0] === 'string') {
      showURLOverlay(results[0]);
    } else {
      showURLOverlay(defaultLink);
    }
  });
}

/**
 * Hides overlay dialog.
 */
function hideOverlay() {
  const doc = appWindow.contentWindow.document;
  const overlayContainer = doc.getElementById('overlay-container');
  overlayContainer.hidden = true;
  if (lastFocusedElement) {
    lastFocusedElement.focus();
    lastFocusedElement = null;
  }
}

function setWindowBounds(x, y, width, height) {
  if (!appWindow) {
    return;
  }

  let outerWidth = OUTER_WIDTH;
  let outerHeight = OUTER_HEIGHT;
  if (outerWidth > width) {
    outerWidth = width;
  }
  if (outerHeight > height) {
    outerHeight = height;
  }

  appWindow.outerBounds.width = outerWidth;
  appWindow.outerBounds.height = outerHeight;
  appWindow.outerBounds.left = Math.ceil(x + (width - outerWidth) / 2);
  appWindow.outerBounds.top = Math.ceil(y + (height - outerHeight) / 2);
}

function closeWindow() {
  if (appWindow) {
    appWindow.close();
  }
}

chrome.app.runtime.onLaunched.addListener(function() {
  const onAppContentLoad = function() {
    const onRetry = function() {
      sendNativeMessage('onRetryClicked');
    };

    const onSendFeedback = function() {
      sendNativeMessage('onSendFeedbackClicked');
    };

    const onRunNetworkTests = function() {
      sendNativeMessage('onRunNetworkTestsClicked');
    };

    const doc = appWindow.contentWindow.document;
    doc.getElementById('button-retry').addEventListener('click', onRetry);
    doc.getElementById('button-send-feedback')
        .addEventListener('click', onSendFeedback);
    doc.getElementById('button-run-network-tests')
        .addEventListener('click', onRunNetworkTests);
    doc.getElementById('overlay-close').addEventListener('click', hideOverlay);
    doc.getElementById('privacy-policy-link')
        .addEventListener('click', showPrivacyPolicyOverlay);

    const overlay = doc.getElementById('overlay-container');
    appWindow.contentWindow.cr.ui.overlay.setupOverlay(overlay);
    appWindow.contentWindow.cr.ui.overlay.globalInitialization();
    overlay.addEventListener('cancelOverlay', hideOverlay);

    const overlayWebview = doc.getElementById('overlay-url');
    overlayWebview.addEventListener('contentload', function() {
      overlay.classList.remove('overlay-loading');
    });
    overlayWebview.addContentScripts([{
      name: 'postProcess',
      matches: ['https://support.google.com/*'],
      css: {files: ['overlay.css']},
      run_at: 'document_end',
    }]);

    focusManager = new appWindow.contentWindow.ArcOptInFocusManager();
    focusManager.initialize();

    connectPort();
    sendNativeMessage('requestWindowBounds');
  };

  const onWindowClosed = function() {
    appWindow = null;

    // Notify to Chrome.
    sendNativeMessage('onWindowClosed');

    // On window closed, then dispose the extension. So, close the port
    // otherwise the background page would be kept alive so that the extension
    // would not be unloaded.
    port.disconnect();
    port = null;
  };

  const onWindowCreated = function(createdWindow) {
    appWindow = createdWindow;
    appWindow.contentWindow.onload = onAppContentLoad;
    appWindow.onClosed.addListener(onWindowClosed);
  };

  const options = {
    'id': 'play_store_wnd',
    'resizable': false,
    'hidden': true,
    'frame': {type: 'chrome', color: '#ffffff'},
    'outerBounds': {'width': OUTER_WIDTH, 'height': OUTER_HEIGHT},
  };
  chrome.app.window.create('main.html', options, onWindowCreated);
});
