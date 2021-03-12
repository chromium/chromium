// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordListItem represents one row in a list of passwords,
 * with a "more actions" button. It needs to be its own component because
 * FocusRowBehavior provides good a11y.
 * Clicking the button fires a password-more-actions-clicked event.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import './passwords_shared_css.js';

import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {ShowPasswordBehavior} from './show_password_behavior.js';

/**
 * @typedef {!Event<!{target: !HTMLElement, listItem:
 *     !PasswordListItemElement}>}
 */
export let PasswordMoreActionsClickedEvent;

Polymer({
  is: 'password-list-item',

  _template: html`{__html_template__}`,

  behaviors: [
    ShowPasswordBehavior,
  ],

  properties: {
    /**
     * Whether to hide the 3 dot button that open the more actions menu.
     */
    shouldHideMoreActionsButton: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Selects the password on tap if revealed.
   * @private
   */
  onReadonlyInputTap_() {
    if (this.password) {
      this.$$('#password').select();
    }
  },

  /**
   * @private
   */
  onPasswordMoreActionsButtonTap_() {
    this.fire(
        'password-more-actions-clicked',
        {target: this.$.moreActionsButton, listItem: this});
  },

  /**
   * Get the aria label for the More Actions button on this row.
   * @private
   */
  getMoreActionsLabel_() {
    // Avoid using I18nBehavior.i18n, because it will filter sequences, which
    // are otherwise not illegal for usernames. Polymer still protects against
    // XSS injection.
    return loadTimeData.getStringF(
        (this.entry.federationText) ? 'passwordRowFederatedMoreActionsButton' :
                                      'passwordRowMoreActionsButton',
        this.entry.username, this.entry.urls.shown);
  },
});
