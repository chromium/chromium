// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// strings.m.js is generated when we enable it via UseStringsJs() in webUI
// controller. When loading it, it will populate data such as localized strings
// into |loadTimeData|.
import './strings.m.js';
import './parent_access_after.js';
import './parent_access_before.js';
import './parent_access_disabled.js';
import './parent_access_ui.js';
import './supervision/supervised_user_error.js';
import './supervision/supervised_user_offline.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessParams_FlowType, ParentAccessResult} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams, getParentAccessUIHandler} from './parent_access_ui_handler.js';

/** @enum {string} */
export const Screens = {
  AUTHENTICATION_FLOW: 'parent-access-ui',
  BEFORE_FLOW: 'parent-access-before',
  AFTER_FLOW: 'parent-access-after',
  DISABLED: 'parent-access-disabled',
  ERROR: 'supervised-user-error',
  OFFLINE: 'supervised-user-offline',
};

/** @enum {string} */
export const ParentAccessEvent = {
  SHOW_AFTER: 'show-after',
  SHOW_AUTHENTICATION_FLOW: 'show-authentication-flow',
  SHOW_ERROR: 'show-error',
  // Individual screens can listen for this event to be notified when the screen
  // becomes active.
  ON_SCREEN_SWITCHED: 'on-screen-switched',
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
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListeners_();
    this.getInitialScreen_().then((initialScreen) => {
      this.switchScreen_(navigator.onLine ? initialScreen : Screens.OFFLINE);
    });
  }

  /** @private */
  addEventListeners_() {
    this.addEventListener(ParentAccessEvent.SHOW_AFTER, () => {
      this.switchScreen_(Screens.AFTER_FLOW);
    });

    this.addEventListener(ParentAccessEvent.SHOW_AUTHENTICATION_FLOW, () => {
      this.switchScreen_(Screens.AUTHENTICATION_FLOW);
      getParentAccessUIHandler().onBeforeScreenDone();
    });

    this.addEventListener(ParentAccessEvent.SHOW_ERROR, () => {
      this.onError_();
    });

    window.addEventListener('online', () => {
      // If the app comes back online, start from the initial screen.
      this.getInitialScreen_().then((initialScreen) => {
        this.switchScreen_(initialScreen);
      });
    });

    window.addEventListener('offline', () => {
      this.switchScreen_(Screens.OFFLINE);
    });
  }

  /** @private */
  async getInitialScreen_() {
    const response = await getParentAccessParams();
    if (response.params.isDisabled) {
      return Screens.DISABLED;
    }
    switch (response.params.flowType) {
      case ParentAccessParams_FlowType.kExtensionAccess:
        return Screens.BEFORE_FLOW;
      case ParentAccessParams_FlowType.kWebsiteAccess:
      default:
        return Screens.AUTHENTICATION_FLOW;
    }
  }

  /**
   * Shows an error screen, which is a terminal state for the flow.
   * @private
   */
  onError_() {
    this.switchScreen_(Screens.ERROR);
    getParentAccessUIHandler().onParentAccessDone(ParentAccessResult.kError);
  }

  /**
   * @param {!Screens} screen
   * @private
   */
  switchScreen_(screen) {
    if (this.isAppInTerminalState_()) {
      return;
    }
    this.currentScreen_ = screen;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
    this.shadowRoot.querySelector(screen).dispatchEvent(
        new CustomEvent(ParentAccessEvent.ON_SCREEN_SWITCHED));
  }

  /**
   * @returns {boolean} If the app can navigate away from the current screen.
   * @private
   */
  isAppInTerminalState_() {
    return this.currentScreen_ === Screens.ERROR ||
        this.currentScreen_ === Screens.DISABLED;
  }
}
customElements.define(ParentAccessApp.is, ParentAccessApp);
