// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './edu_login_css.js';
import './edu_login_template.js';
import './edu_login_button.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {getImage} from 'chrome://resources/js/icon.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EduAccountLoginBrowserProxyImpl} from './browser_proxy.js';
import {EduLoginErrorType, ParentAccount} from './edu_login_util.js';

Polymer({
  is: 'edu-login-parents',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Selected parent account for approving EDU login flow.
     * @type {?ParentAccount}
     */
    selectedParent: {
      type: Object,
      value: null,
      notify: true,
    },

    /**
     * Array of user's parents.
     * @private {!Array<!ParentAccount>}
     */
    parents_: {
      type: Array,
      value() {
        return [];
      },
    },

    /**
     * Whether current flow is a reauthentication.
     * @private {boolean}
     */
    reauthFlow_: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  created() {
    // For the reauth flow the email is appended to query params in
    // InlineLoginHandlerDialogChromeOS. It's used later in auth extension to
    // pass the email value to Gaia.
    let currentQueryParameters = new URLSearchParams(window.location.search);
    if (currentQueryParameters.get('email')) {
      this.reauthFlow_ = true;
    }
    document.title = this.getTitle_();
  },

  /** @override */
  ready() {
    EduAccountLoginBrowserProxyImpl.getInstance().isNetworkReady().then(
        result => {
          if (result) {
            this.getParents_();
            return;
          }
          // No internet connection.
          this.fire(
              'edu-login-error', {errorType: EduLoginErrorType.NO_INTERNET});
        });
  },

  /** @private */
  getParents_() {
    EduAccountLoginBrowserProxyImpl.getInstance().getParents().then(
        result => {
          this.parents_ = result;
        },
        error => {
          this.fire(
              'edu-login-error',
              {errorType: EduLoginErrorType.CANNOT_ADD_ACCOUNT});
        });
  },

  /**
   * @param {ParentAccount} selectedParent
   * @param {ParentAccount} item
   * @return {string} class name
   * @private
   */
  getSelectedClass_(selectedParent, item) {
    return item === selectedParent ? 'selected' : '';
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return getImage(iconUrl);
  },

  /**
   * @param {ParentAccount} selectedParent
   * @return {boolean}
   * @private
   */
  isNextDisabled_(selectedParent) {
    return selectedParent === null;
  },

  /**
   * Sets selected parent.
   * @param {Event} e
   * @private
   */
  onParentSelectionKeydown_(e) {
    if (e.key === 'Enter' || e.key === ' ') {  // Enter or Space key
      this.selectParent_(e);
    }
  },

  /**
   * @param {Event} e
   * @private
   */
  selectParent_(e) {
    this.selectedParent = e.model.item;
    this.fire('go-next');
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_() {
    return this.i18n('parentsListTitle');
  },

  /**
   * @return {string}
   * @private
   */
  getBody_() {
    return this.i18n('parentsListBody');
  },

  /**
   * @return {string}
   * @private
   */
  getReauthMessage_() {
    return this.i18n('reauthBody');
  }
});
