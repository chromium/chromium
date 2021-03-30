// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordCheckListItem represents one insecure credential in the
 * list of insecure passwords.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/js/action_link.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import './passwords_shared_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';

// <if expr="chromeos">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

Polymer({
  is: 'password-check-list-item',

  _template: html`{__html_template__}`,

  properties: {
    // <if expr="chromeos">
    /** @type {BlockingRequestManager} */
    tokenRequestManager: Object,
    // </if>

    /**
     * The password that is being displayed.
     * @type {!PasswordManagerProxy.InsecureCredential}
     */
    item: Object,

    /** @private */
    isPasswordVisible_: {
      type: Boolean,
      computed: 'computePasswordVisibility_(item.password)',
    },

    /** @private */
    password_: {
      type: String,
      computed: 'computePassword_(item.password)',
    },

    clickedChangePassword: {
      type: Boolean,
      value: false,
    },

    /** @private */
    buttonClass_: {
      type: String,
      computed: 'computeButtonClass_(item.compromisedInfo)',
    },

    /** @private */
    iconClass_: {
      type: String,
      computed: 'computeIconClass_(item.compromisedInfo)',
    },
  },

  /**
   * @private {?PasswordManagerProxy}
   */
  passwordManager_: null,

  /** @override */
  attached() {
    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();
  },

  /**
   * Returns true if |item| is compromised credential, otherwise returns false.
   * @return {boolean}
   * @private
   */
  isCompromisedItem_() {
    return !!this.item.compromisedInfo;
  },

  /**
   * @return {string}
   * @private
   */
  getCompromiseType_() {
    switch (this.item.compromisedInfo.compromiseType) {
      case chrome.passwordsPrivate.CompromiseType.PHISHED:
        return loadTimeData.getString('phishedPassword');
      case chrome.passwordsPrivate.CompromiseType.LEAKED:
        return loadTimeData.getString('leakedPassword');
      case chrome.passwordsPrivate.CompromiseType.PHISHED_AND_LEAKED:
        return loadTimeData.getString('phishedAndLeakedPassword');
    }

    assertNotReached(
        'Can\'t find a string for type: ' +
        this.item.compromisedInfo.compromiseType);
  },

  /**
   * @private
   */
  onChangePasswordClick_() {
    this.fire('change-password-clicked', {id: this.item.id});

    const url = assert(this.item.changePasswordUrl);
    OpenWindowProxyImpl.getInstance().openURL(url);

    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.CHANGE_PASSWORD);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMoreClick_(event) {
    this.fire('more-actions-click', {moreActionsButton: event.target});
  },

  /**
   * @return {string}
   * @private
   */
  getInputType_() {
    return this.isPasswordVisible_ ? 'text' : 'password';
  },

  /**
   * @return {boolean}
   * @private
   */
  computePasswordVisibility_() {
    return !!this.item.password;
  },

  /**
   * @return {string}
   * @private
   */
  computeButtonClass_() {
    if (this.item.compromisedInfo) {
      // Strong CTA.
      return 'action-button';
    }
    // Weak CTA.
    return '';
  },

  /**
   * @return {string}
   * @private
   */
  computeIconClass_() {
    if (this.item.compromisedInfo) {
      // Strong CTA, white icon.
      return '';
    }
    // Weak CTA, non-white-icon.
    return 'icon-weak-cta';
  },

  /**
   * @return {string}
   * @private
   */
  computePassword_() {
    const NUM_PLACEHOLDERS = 10;
    return this.item.password || ' '.repeat(NUM_PLACEHOLDERS);
  },

  /**
   * @public
   */
  hidePassword() {
    this.set('item.password', null);
  },

  /**
   * @public
   */
  showPassword() {
    this.passwordManager_.recordPasswordCheckInteraction(
        PasswordManagerProxy.PasswordCheckInteraction.SHOW_PASSWORD);
    this.passwordManager_
        .getPlaintextInsecurePassword(
            assert(this.item), chrome.passwordsPrivate.PlaintextReason.VIEW)
        .then(
            insecureCredential => {
              this.set('item', insecureCredential);
            },
            error => {
              // <if expr="chromeos">
              // If no password was found, refresh auth token and retry.
              this.tokenRequestManager.request(this.showPassword.bind(this));
              // </if>
            });
  },

  /**
   * @private
   */
  onReadonlyInputTap_() {
    if (this.isPasswordVisible_) {
      this.$$('#leakedPassword').select();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onAlreadyChangedClick_(event) {
    event.preventDefault();
    this.fire('already-changed-password-click', event.target);
  },
});
