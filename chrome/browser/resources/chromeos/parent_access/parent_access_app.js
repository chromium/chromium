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
import './parent_access_error.js';
import './parent_access_offline.js';
import './parent_access_ui.js';
import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ParentAccessParams_FlowType, ParentAccessResult} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams, getParentAccessUIHandler} from './parent_access_ui_handler.js';

/** @enum {string} */
export const Screens = {
  AUTHENTICATION_FLOW: 'parent-access-ui',
  BEFORE_FLOW: 'parent-access-before',
  AFTER_FLOW: 'parent-access-after',
  DISABLED: 'parent-access-disabled',
  ERROR: 'parent-access-error',
  OFFLINE: 'parent-access-offline',
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

/**
 * Returns true if the Parent Access Jelly feature flag is enabled.
 * @return {boolean}
 */
export function isParentAccessJellyEnabled() {
  return loadTimeData.valueExists('isParentAccessJellyEnabled') &&
      loadTimeData.getBoolean('isParentAccessJellyEnabled');
}

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

    // TODO (b/297564545): Clean up Jelly flag logic after Jelly is enabled.
    if (isParentAccessJellyEnabled()) {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = 'chrome://theme/colors.css?sets=legacy,sys';
      document.head.appendChild(link);
      document.body.classList.add('jelly-enabled');
      /** @suppress {checkTypes} */
      (function() {
        ColorChangeUpdater.forDocument().start();
      })();
    }

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
