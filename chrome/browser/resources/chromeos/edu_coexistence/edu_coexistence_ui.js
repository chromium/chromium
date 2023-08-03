// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './gaia_action_buttons/gaia_action_buttons.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from '//resources/ash/common/web_ui_listener_behavior.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';
import {EduCoexistenceController} from './edu_coexistence_controller.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {WebUIListenerBehaviorInterface}
 */
const EduCoexistenceUiBase =
    mixinBehaviors([WebUIListenerBehavior], PolymerElement);

/**
 * @polymer
 */
class EduCoexistenceUi extends EduCoexistenceUiBase {
  static get is() {
    return 'edu-coexistence-ui';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Indicates whether the page is loading.
       * @private
       */
      loading_: {
        type: Boolean,
        value: true,
      },

      /**
       * Indicates whether the GAIA buttons should be shown.
       * @private
       */
      showGaiaButtons_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the GAIA "Next" button should be shown.
       * @private
       */
      showGaiaNextButton_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates whether the new OOBE Layout should be enabled. For
       * simplicity, this only controls whether particular elements are
       * rendered, and does not prevent the new oobe adaptive layout features,
       * which will always be enabled.
       * @private
       */
      newOobeLayoutEnabled_: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates the CSS class used for the buttons-layout for
       * the buttons at the bottom of the screen.  The layout
       * differs depending on whether the new OOBE layout is enabled.
       * @private
       */
      buttonsLayoutCssClass_: {
        type: String,
        computed: 'getButtonsCssClass_(newOobeLayoutEnabled_)',
      },

      /**
       * The EDU Coexistence controller instance.
       * @private
       */
      controller_: Object,
    };
  }

  constructor() {
    super();
    this.webview_ = null;
  }

  /** @override */
  ready() {
    super.ready();
    this.addWebUIListener(
        'load-auth-extension', data => this.loadAuthExtension_(data));
    this.webview_ = this.$.signinFrame;

    this.webview_.addEventListener('loadabort', () => {
      this.loading_ = false;
      this.showError_();
    });

    EduCoexistenceBrowserProxyImpl.getInstance().initializeEduArgs().then(
        (data) => {
          this.controller_ =
              new EduCoexistenceController(this, assert(this.webview_), data);
          this.newOobeLayoutEnabled_ =
              this.controller_.getNewOobeLayoutEnabled();
          EduCoexistenceBrowserProxyImpl.getInstance().initializeLogin();
        },
        (err) => {
          this.showError_();
          EduCoexistenceBrowserProxyImpl.getInstance().onError(
              ['There was an error getting edu coexistence data']);
        });
  }

  /** @private */
  showError_() {
    this.dispatchEvent(new CustomEvent('go-error', {
      bubbles: true,
      composed: true,
    }));
  }

  /**
   * Attempts to close the dialog
   * @private
   */
  closeDialog_() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  }

  /** @private */
  loadAuthExtension_(data) {
    // Set up the controller.
    this.controller_.loadAuthExtension(data);

    this.webview_.addEventListener('contentload', () => {
      this.loading_ = false;
      this.configureUiForGaiaFlow_();
    });
  }

  /** @private */
  handleGaiaLoginGoBack_(e) {
    e.stopPropagation();
    const backButton = this.root.getElementById('gaia-back-button');
    if (backButton.disabled) {
      // This is a safeguard against this method getting called somehow
      // despite the button being disabled.
      return;
    }
    backButton.disabled = true;

    this.webview_.back((success /* ignored */) => {
      // Wait a full second after the callback fires before processing another
      // click on the back button.  This delay is needed because the callback
      // fires before the content finishes navigating to the previous page.
      setTimeout(() => {
        backButton.disabled = false;
      }, 1000 /* 1 second */);
      this.webview_.focus();
    });
  }

  /** @private */
  getButtonsCssClass_(newOobeLayoutEnabled) {
    return newOobeLayoutEnabled ? 'new-oobe-buttons-layout' : 'buttons-layout';
  }

  /**
   * Configures the UI for showing/hiding the GAIA login flow.
   * @private
   */
  configureUiForGaiaFlow_() {
    const currentUrl = new URL(this.webview_.src);
    const template = this.shadowRoot.querySelector('edu-coexistence-template');
    const contentContainer = template.$$('div.content-container');

    if (currentUrl.hostname !== this.controller_.getFlowOriginHostname()) {
      this.shadowRoot.querySelector('edu-coexistence-button')
          .newOobeStyleEnabled = this.newOobeLayoutEnabled_;

      this.shadowRoot.querySelector('gaia-action-buttons').roundedButton =
          this.newOobeLayoutEnabled_;

      // Show the GAIA Buttons.
      this.showGaiaButtons_ = true;
      // Shrink the content-container so that the buttons line up more closely
      // with the server rendered buttons.
      contentContainer.style.height = 'calc(100% - 90px)';

      // Don't show the "Next" button if the EDU authentication got forwarded to
      // a non-Google SSO page.
      this.showGaiaNextButton_ = currentUrl.hostname.endsWith('.google.com');
    } else {
      // Hide the GAIA Buttons.
      this.showGaiaButtons_ = false;

      // Hide the GAIA Next button.
      this.showGaiaNextButton_ = false;

      // Restore the content container div to 100%
      contentContainer.style.height = '100%';
    }

    template.showButtonFooter(this.showGaiaButtons_);
  }
}

customElements.define(EduCoexistenceUi.is, EduCoexistenceUi);
