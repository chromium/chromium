// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-autofill-page' is the settings page containing settings for
 * passwords, payment methods and addresses.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import '../prefs/prefs.m.js';
import '../settings_page/settings_animated_pages.m.js';
import '../settings_page/settings_subpage.m.js';
import '../settings_shared_css.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

import {PasswordCheckBehavior} from './password_check_behavior.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

Polymer({
  is: 'settings-autofill-page',

  _template: html`{__html_template__}`,

  behaviors: [
    PrefsBehavior,
    PasswordCheckBehavior,
  ],

  properties: {
    /** @private Filter applied to passwords and password exceptions. */
    passwordFilter_: String,

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value() {
        const map = new Map();
        if (routes.PASSWORDS) {
          map.set(routes.PASSWORDS.path, '#passwordManagerButton');
        }
        if (routes.PAYMENTS) {
          map.set(routes.PAYMENTS.path, '#paymentManagerButton');
        }
        if (routes.ADDRESSES) {
          map.set(routes.ADDRESSES.path, '#addressesManagerButton');
        }

        return map;
      },
    },

    /** @private */
    passwordManagerSubLabel_: {
      type: String,
      computed: 'computePasswordManagerSubLabel_(compromisedPasswordsCount)',
    },
  },

  /**
   * Shows the manage addresses sub page.
   * @param {!Event} event
   * @private
   */
  onAddressesClick_(event) {
    Router.getInstance().navigateTo(routes.ADDRESSES);
  },

  /**
   * Shows the manage payment methods sub page.
   * @private
   */
  onPaymentsClick_() {
    Router.getInstance().navigateTo(routes.PAYMENTS);
  },

  /**
   * Shows a page to manage passwords.
   * @private
   */
  onPasswordsClick_() {
    PasswordManagerImpl.getInstance().recordPasswordsPageAccessInSettings();
    Router.getInstance().navigateTo(routes.PASSWORDS);
  },

  /**
   * @return {string} The sub-title message indicating the result of password
   *     check.
   * @private
   */
  computePasswordManagerSubLabel_() {
    return this.leakedPasswords.length > 0 ? this.compromisedPasswordsCount :
                                             '';
  },
});
