// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_coexistence_css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './edu_coexistence_error.js';
import './edu_coexistence_offline.js';
import './edu_coexistence_ui.js';
import './arc_account_picker/arc_account_picker_app.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';

import {getAccountAdditionOptionsFromJSON} from './arc_account_picker/arc_util.js';
import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';

/** @enum {string} */
export const Screens = {
  ONLINE_FLOW: 'edu-coexistence-ui',
  ERROR: 'edu-coexistence-error',
  OFFLINE: 'edu-coexistence-offline',
  ARC_ACCOUNT_PICKER: 'arc-account-picker',
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

    /*
     * True if `kArcAccountRestrictions` feature is enabled.
     * @private
     */
    isArcAccountRestrictionsEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isArcAccountRestrictionsEnabled');
      },
      readOnly: true,
    },

    /**
     * Specifies what the current screen is.
     * @private {Screens}
     */
    currentScreen_: {type: Screens},
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
    this.dispatchEvent(new CustomEvent('switch-view-notify-for-testing'));
  },

  /**
   * @param {boolean} isOnline Whether or not the browser is online.
   * @private
   */
  setInitialScreen_(isOnline) {
    const initialScreen = isOnline ? Screens.ONLINE_FLOW : Screens.OFFLINE;
    if (this.isArcAccountRestrictionsEnabled_) {
      const options = getAccountAdditionOptionsFromJSON(
          EduCoexistenceBrowserProxyImpl.getInstance().getDialogArguments());
      if (!!options && options.showArcAvailabilityPicker) {
        this.$$('arc-account-picker-app')
            .loadAccounts()
            .then(
                accountsFound => {
                  this.switchToScreen_(
                      accountsFound ? Screens.ARC_ACCOUNT_PICKER :
                                      initialScreen);
                },
                reject => {
                  this.switchToScreen_(initialScreen);
                });
        return;
      }
    }
    this.switchToScreen_(initialScreen);
  },

  /**
   * Switches to 'Add account' flow.
   * @private
   */
  showAddAccount_() {
    this.switchToScreen_(
        navigator.onLine ? Screens.ONLINE_FLOW : Screens.OFFLINE);
  },

  /**
   * Attempts to close the dialog.
   * @private
   */
  closeDialog_() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  },

  /** @override */
  ready() {
    this.addWebUIListener('show-error-screen', () => {
      this.onError_();
    });

    window.addEventListener('online', () => {
      if (this.currentScreen_ !== Screens.ERROR &&
          this.currentScreen_ !== Screens.ARC_ACCOUNT_PICKER) {
        this.switchToScreen_(Screens.ONLINE_FLOW);
      }
    });

    window.addEventListener('offline', () => {
      if (this.currentScreen_ !== Screens.ERROR &&
          this.currentScreen_ !== Screens.ARC_ACCOUNT_PICKER) {
        this.switchToScreen_(Screens.OFFLINE);
      }
    });
    this.setInitialScreen_(navigator.onLine);
  },

});
