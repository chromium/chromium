// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import './account_manager_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';
import {getAccountAdditionOptionsFromJSON} from './inline_login_util.js';

Polymer({
  is: 'welcome-page-app',

  _template: html`{__html_template__}`,

  properties: {
    /* The value of the 'available in ARC' toggle.*/
    isAvailableInArc: {
      type: Boolean,
      notify: true,
    },

    /*
     * True if the 'Add account' flow is opened from ARC.
     * In this case we will hide the toggle and show different welcome message.
     * @private
     */
    isArcFlow_: {
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
  },

  /** @override */
  ready() {
    if (this.isArcAccountRestrictionsEnabled_) {
      const options = getAccountAdditionOptionsFromJSON(
          InlineLoginBrowserProxyImpl.getInstance().getDialogArguments());
      if (!options) {
        // Options are not available during reauthentication.
        return;
      }

      // Set the default value.
      this.isAvailableInArc = options.isAvailableInArc;
      if (options.showArcAvailabilityPicker) {
        this.isArcFlow_ = true;
        assert(this.isAvailableInArc);
      }
    }

    this.setUpLinkCallbacks_();
  },

  /** @return {boolean} */
  isSkipCheckboxChecked() {
    return !!this.$.checkbox && this.$.checkbox.checked;
  },

  /** @private */
  setUpLinkCallbacks_() {
    [this.$$('#osSettingsLink'), this.$$('#appsSettingsLink'),
     this.$$('#newPersonLink')]
        .filter(link => !!link)
        .forEach(link => {
          link.addEventListener(
              'click',
              () => this.dispatchEvent(new CustomEvent('opened-new-window')));
        });

    if (this.isArcAccountRestrictionsEnabled_) {
      const guestModeLink = this.$$('#guestModeLink');
      if (guestModeLink) {
        guestModeLink.addEventListener('click', () => this.openGuestLink_());
      }
    } else {
      const incognitoLink = this.$$('#incognitoLink');
      if (incognitoLink) {
        incognitoLink.addEventListener(
            'click', () => this.openIncognitoLink_());
      }
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  isArcToggleVisible_() {
    return this.isArcAccountRestrictionsEnabled_ && !this.isArcFlow_;
  },

  /**
   * @return {string}
   * @private
   */
  getWelcomeTitle_() {
    return loadTimeData.getStringF(
        'accountManagerDialogWelcomeTitle', loadTimeData.getString('userName'));
  },

  /**
   * @return {string}
   * @private
   */
  getWelcomeBody_() {
    const welcomeBodyKey =
        (this.isArcAccountRestrictionsEnabled_ && this.isArcFlow_) ?
        'accountManagerDialogWelcomeBodyArc' :
        'accountManagerDialogWelcomeBody';
    return loadTimeData.getString(welcomeBodyKey);
  },

  /** @private */
  openIncognitoLink_() {
    InlineLoginBrowserProxyImpl.getInstance().showIncognito();
    // `showIncognito` will close the dialog.
  },

  /** @private */
  openGuestLink_() {
    InlineLoginBrowserProxyImpl.getInstance().openGuestWindow();
    // `openGuestWindow` will close the dialog.
  },
});
