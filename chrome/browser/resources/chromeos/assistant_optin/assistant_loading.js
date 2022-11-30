// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * loading screen.
 *
 * Event 'reload' will be fired when the user click the retry button.
 */

/* #js_imports_placeholder */

const AssistantLoadingUIState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  ERROR: 'error',
};

/**
 * @constructor
 * @extends {PolymerElement}
 */
const AssistantLoadingBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, MultiStepBehavior], Polymer.Element);

/**
 * @polymer
 */
class AssistantLoading extends AssistantLoadingBase {
  static get is() {
    return 'assistant-loading';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * Buttons are disabled when the page content is loading.
       */
      buttonsDisabled: {
        type: Boolean,
        value: true,
      },
    };
  }

  constructor() {
    super();

    this.UI_STEPS = AssistantLoadingUIState;

    /**
     * Whether an error occurs while the page is loading.
     * @type {boolean}
     * @private
     */
    this.loadingError_ = false;

    /**
     * Timeout ID for loading animation.
     * @type {number}
     * @private
     */
    this.animationTimeout_ = null;

    /**
     * Timeout ID for loading (will fire an error).
     * @type {number}
     * @private
     */
    this.loadingTimeout_ = null;

    /** @private {?assistant.BrowserProxy} */
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  }

  defaultUIStep() {
    return AssistantLoadingUIState.LOADED;
  }


  /**
   * On-tap event handler for retry button.
   *
   * @private
   */
  onRetryTap_() {
    this.dispatchEvent(
        new CustomEvent('reload', {bubbles: true, composed: true}));
  }

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_() {
    if (this.buttonsDisabled) {
      return;
    }
    this.buttonsDisabled = true;
    this.browserProxy_.flowFinished();
  }

  /**
   * Reloads the page.
   */
  reloadPage() {
    window.clearTimeout(this.animationTimeout_);
    window.clearTimeout(this.loadingTimeout_);
    this.setUIStep(AssistantLoadingUIState.LOADED);
    this.buttonsDisabled = true;
    this.animationTimeout_ = window.setTimeout(
        () => this.setUIStep(AssistantLoadingUIState.LOADING), 500);
    this.loadingTimeout_ =
        window.setTimeout(() => this.onLoadingTimeout(), 15000);
  }

  /**
   * Handles event when page content cannot be loaded.
   */
  onErrorOccurred(details) {
    this.loadingError_ = true;
    window.clearTimeout(this.animationTimeout_);
    window.clearTimeout(this.loadingTimeout_);
    this.setUIStep(AssistantLoadingUIState.ERROR);

    this.buttonsDisabled = false;
    this.$['retry-button'].focus();
  }

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded() {
    window.clearTimeout(this.animationTimeout_);
    window.clearTimeout(this.loadingTimeout_);
    this.setUIStep(AssistantLoadingUIState.LOADED);
  }

  /**
   * Called when the loading timeout is triggered.
   */
  onLoadingTimeout() {
    this.browserProxy_.timeout();
    this.onErrorOccurred();
  }

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    this.reloadPage();
    Polymer.RenderStatus.afterNextRender(
        this, () => this.$['loading-dialog'].focus());
  }
}

customElements.define(AssistantLoading.is, AssistantLoading);
