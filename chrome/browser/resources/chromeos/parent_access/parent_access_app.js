// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// strings.m.js is generated when we enable it via UseStringsJs() in webUI
// controller. When loading it, it will populate data such as localized strings
// into |loadTimeData|.
import './strings.m.js';
import './parent_access_after.js';
import './parent_access_before.js';
import './parent_access_ui.js';
import './supervision/supervised_user_error.js';
import './supervision/supervised_user_offline.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessResult} from './parent_access_ui.mojom-webui.js';
import {getParentAccessUIHandler} from './parent_access_ui_handler.js';

/** @enum {string} */
export const Screens = {
  AUTHENTICATION_FLOW: 'parent-access-ui',
  BEFORE_FLOW: 'parent-access-before',
  AFTER_FLOW: 'parent-access-after',
  ERROR: 'supervised-user-error',
  OFFLINE: 'supervised-user-offline',
};

class ParentAccessApp extends PolymerElement {
  static get is() {
    return 'parent-access-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Specifies what the current screen is.
       * @private {Screens}
       */
      currentScreen_: {
        type: Screens,
        value: Screens.AUTHENTICATION_FLOW,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListener('show-after', () => {
      this.currentScreen_ = Screens.AFTER_FLOW;
      /** @type {CrViewManagerElement} */ (this.$.viewManager)
          .switchView(this.currentScreen_);
      this.shadowRoot.querySelector('parent-access-after').onShowAfterScreen();
    });

    this.addEventListener('show-authentication-flow', () => {
      this.currentScreen_ = Screens.AUTHENTICATION_FLOW;
      /** @type {CrViewManagerElement} */ (this.$.viewManager)
          .switchView(this.currentScreen_);
    });

    this.addEventListener('show-error', () => {
      this.onError_();
    });

    window.addEventListener('online', () => {
      if (this.currentScreen_ !== Screens.ERROR) {
        this.currentScreen_ = this.getInitialScreen_();
        /** @type {CrViewManagerElement} */ (this.$.viewManager)
            .switchView(this.currentScreen_);
      }
    });

    window.addEventListener('offline', () => {
      if (this.currentScreen_ !== Screens.ERROR) {
        this.currentScreen_ = Screens.OFFLINE;
        /** @type {CrViewManagerElement} */ (this.$.viewManager)
            .switchView(this.currentScreen_);
      }
    });

    this.currentScreen_ =
        navigator.onLine ? this.getInitialScreen_() : Screens.OFFLINE;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
  }

  getInitialScreen_() {
    // TODO(b/262448394): Implement logic to check if the before screen should
    // be shown instead of the authentication flow.
    return Screens.AUTHENTICATION_FLOW;
  }

  /**
   * Shows an error screen, which is a terminal state for the flow.
   * @private
   */
  onError_() {
    this.currentScreen_ = Screens.ERROR;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
    getParentAccessUIHandler().onParentAccessDone(ParentAccessResult.kError);
  }
}
customElements.define(ParentAccessApp.is, ParentAccessApp);
