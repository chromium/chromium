// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './edu_coexistence_error.js';
import './edu_coexistence_offline.js';
import './edu_coexistence_ui.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';

/** @enum {string} */
export const Screens = {
  ONLINE_FLOW: 'edu-coexistence-ui',
  ERROR: 'edu-coexistence-error',
  OFFLINE: 'edu-coexistence-offline',
};

Polymer({
  is: 'edu-coexistence-app',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Whether the error screen should be shown.
     * @private {boolean}
     */
    isErrorShown_: {
      type: Boolean,
      value: false,
    },

    /**
     * Specifies what the current screen is.
     * @private {Screens}
     */
    currentScreen_: {type: Screens, value: Screens.ONLINE_FLOW},
  },

  listeners: {
    'go-error': 'onError_',
  },

  /**
   * Displays the error screen.
   * @private
   */
  onError_() {
    this.switchToScreen_(Screens.ERROR);
  },

  /**
   * Switches to the specified screen.
   * @private
   * @param {Screens} screen
   */
  switchToScreen_(screen) {
    if (this.currentScreen_ === screen) {
      return;
    }
    this.currentScreen_ = screen;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
  },

  /**
   * @param {boolean} isOnline Whether or not the browser is online.
   * @private
   */
  setInitialScreen_(isOnline) {
    this.currentScreen_ = isOnline ? Screens.ONLINE_FLOW : Screens.OFFLINE;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
  },

  /** @override */
  ready() {
    this.addWebUIListener('show-error-screen', () => {
      this.onError_();
    });

    window.addEventListener('online', () => {
      if (this.currentScreen_ !== Screens.ERROR) {
        this.switchToScreen_(Screens.ONLINE_FLOW);
      }
    });

    window.addEventListener('offline', () => {
      if (this.currentScreen_ !== Screens.ERROR) {
        this.switchToScreen_(Screens.OFFLINE);
      }
    });
    this.setInitialScreen_(navigator.onLine);
  },

});
