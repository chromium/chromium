// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * loading screen.
 *
 * Event 'reload' will be fired when the user click the retry button.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../components/buttons/oobe_text_button.js';
import '../components/common_styles/oobe_dialog_host_styles.css.js';
import '../components/dialogs/oobe_adaptive_dialog.js';
import '../components/dialogs/oobe_content_dialog.js';
import './assistant_icons.html.js';
import './assistant_common_styles.css.js';

import {afterNextRender, html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStepMixin} from '../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../components/mixins/oobe_i18n_mixin.js';

import {BrowserProxyImpl} from './browser_proxy.js';


const AssistantLoadingUIState = {
  LOADING: 'loading',
  LOADED: 'loaded',
  ERROR: 'error',
};

/**
 * @constructor
 * @extends {PolymerElement}
 */
const AssistantLoadingBase = MultiStepMixin(OobeI18nMixin(PolymerElement));

/**
 * @polymer
 */
class AssistantLoading extends AssistantLoadingBase {
  static get is() {
    return 'assistant-loading';
  }

  static get template() {
    return html`{__html_template__}`;
  }

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

    /** @private {?BrowserProxy} */
    this.browserProxy_ = BrowserProxyImpl.getInstance();
  }

  /** @override */
  UI_STEPS() {
    return AssistantLoadingUIState;
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
    afterNextRender(this, () => this.$['loading-dialog'].focus());
  }
}

customElements.define(AssistantLoading.is, AssistantLoading);
