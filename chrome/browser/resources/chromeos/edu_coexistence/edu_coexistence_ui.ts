// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './common.css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import '../gaia_action_buttons/gaia_action_buttons.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {AuthParams} from 'chrome://chrome-signin/gaia_auth_host/authenticator.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';
import {EduCoexistenceButton} from './edu_coexistence_button.js';
import {EduCoexistenceController, EduCoexistenceParams} from './edu_coexistence_controller.js';
import {getTemplate} from './edu_coexistence_ui.html.js';

export interface EduCoexistenceUi {
  $: {
    signinFrame: chrome.webviewTag.WebView,
  };
}

const EduCoexistenceUiBase = WebUiListenerMixin(PolymerElement);

export class EduCoexistenceUi extends EduCoexistenceUiBase {
  static get is() {
    return 'edu-coexistence-ui';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Indicates whether the page is loading. */
      loading: {
        type: Boolean,
        value: true,
      },

      /** Indicates whether the GAIA buttons should be shown. */
      showGaiaButtons: {
        type: Boolean,
        value: false,
      },

      /** Indicates whether the GAIA "Next" button should be shown. */
      showGaiaNextButton: {
        type: Boolean,
        value: false,
      },

      /** The EDU Coexistence controller instance. */
      controller: Object,
    };
  }

  loading: boolean;
  showGaiaButtons: boolean;
  showGaiaNextButton: boolean;
  private webview: chrome.webviewTag.WebView;
  private controller: EduCoexistenceController;

  override ready() {
    super.ready();
    this.addWebUiListener(
        'load-authenticator',
        (data: AuthParams) => this.loadAuthenticator(data));
    this.webview = this.$.signinFrame;

    this.webview.addEventListener('loadabort', () => {
      this.loading = false;
      this.showError();
    });

    EduCoexistenceBrowserProxyImpl.getInstance().initializeEduArgs().then(
        (data: EduCoexistenceParams) => {
          this.controller =
              new EduCoexistenceController(this, assert(this.webview), data);
          EduCoexistenceBrowserProxyImpl.getInstance().initializeLogin();
        },
        () => {
          this.showError();
          EduCoexistenceBrowserProxyImpl.getInstance().onError(
              ['There was an error getting edu coexistence data']);
        });
  }

  setWebviewForTest(webview: chrome.webviewTag.WebView) {
    this.webview = webview;
  }

  private showError() {
    this.dispatchEvent(new CustomEvent('go-error', {
      bubbles: true,
      composed: true,
    }));
  }

  private closeDialog() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  }

  private loadAuthenticator(data: AuthParams) {
    // Set up the controller.
    this.controller.loadAuthenticator(data);

    this.webview.addEventListener('contentload', () => {
      this.loading = false;
      this.configureUiForGaiaFlow();
    });
  }

  private handleGaiaLoginGoBack(e: Event) {
    e.stopPropagation();
    const backButton = this.shadowRoot!.querySelector<EduCoexistenceButton>(
        '#gaia-back-button')!;
    if (backButton!.disabled) {
      // This is a safeguard against this method getting called somehow
      // despite the button being disabled.
      return;
    }
    backButton!.disabled = true;

    this.webview.back(() => {
      // Wait a full second after the callback fires before processing another
      // click on the back button.  This delay is needed because the callback
      // fires before the content finishes navigating to the previous page.
      setTimeout(() => {
        backButton!.disabled = false;
      }, 1000 /* 1 second */);
      this.webview.focus();
    });
  }

  /**
   * Configures the UI for showing/hiding the GAIA login flow.
   */
  private configureUiForGaiaFlow() {
    const currentUrl = new URL(this.webview.src);
    const template =
        this.shadowRoot!.querySelector('edu-coexistence-template')!;
    // const contentContainer = template!.getContentContainer();
    const contentContainer =
        template.shadowRoot!.querySelector<HTMLElement>('.content-container')!;

    if (currentUrl.hostname !== this.controller.getFlowOriginHostname()) {
      // Show the GAIA Buttons.
      this.showGaiaButtons = true;
      // Shrink the content-container so that the buttons line up more closely
      // with the server rendered buttons.
      contentContainer.style.height = 'calc(100% - 90px)';

      // Don't show the "Next" button if the EDU authentication got forwarded to
      // a non-Google SSO page.
      this.showGaiaNextButton = currentUrl.hostname.endsWith('.google.com');
    } else {
      // Hide the GAIA Buttons.
      this.showGaiaButtons = false;

      // Hide the GAIA Next button.
      this.showGaiaNextButton = false;

      // Restore the content container div to 100%
      contentContainer.style.height = '100%';
    }

    template!.updateButtonFooterVisibility(this.showGaiaButtons);
  }
}

customElements.define(EduCoexistenceUi.is, EduCoexistenceUi);
