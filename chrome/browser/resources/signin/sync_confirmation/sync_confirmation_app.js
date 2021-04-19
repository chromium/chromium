/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './strings.m.js';
import './signin_shared_css.js';
import './signin_vars_css.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SyncConfirmationBrowserProxy, SyncConfirmationBrowserProxyImpl} from './sync_confirmation_browser_proxy.js';

Polymer({
  is: 'sync-confirmation-app',

  _template: html`{__html_template__}`,

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /** @private */
    accountImageSrc_: {
      type: String,
      value() {
        return loadTimeData.getString('accountPictureUrl');
      },
    },

    /** @private */
    isProfileCreationFlow_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isProfileCreationFlow');
      }
    },

    /** @private */
    highlightColor_: {
      type: String,
      value() {
        if (!loadTimeData.valueExists('highlightColor')) {
          return '';
        }

        return loadTimeData.getString('highlightColor');
      }
    },

    /** @private */
    showEnterpriseBadge_: {
      type: Boolean,
      value: false,
    }
  },

  /** @private {?SyncConfirmationBrowserProxy} */
  syncConfirmationBrowserProxy_: null,

  /** @override */
  attached() {
    this.syncConfirmationBrowserProxy_ =
        SyncConfirmationBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'account-info-changed', this.handleAccountInfoChanged_.bind(this));
    this.syncConfirmationBrowserProxy_.requestAccountInfo();
  },

  /** @private */
  onConfirm_(e) {
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path));
  },

  /** @private */
  onUndo_() {
    this.syncConfirmationBrowserProxy_.undo();
  },

  /** @private */
  onGoToSettings_(e) {
    this.syncConfirmationBrowserProxy_.goToSettings(
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path));
  },

  /**
   * @param {!Array<!HTMLElement>} path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return {string} The text of the consent confirmation element.
   * @private
   */
  getConsentConfirmation_(path) {
    for (const element of path) {
      if (element.nodeType !== Node.DOCUMENT_FRAGMENT_NODE &&
          element.hasAttribute('consent-confirmation')) {
        return element.innerHTML.trim();
      }
    }
    assertNotReached('No consent confirmation element found.');
    return '';
  },

  /** @return {!Array<string>} Text of the consent description elements. */
  getConsentDescription_() {
    const consentDescription =
        Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription.length);
    return consentDescription;
  },

  /**
   * Called when the account image changes.
   * @param {{
   *   src: string,
   *   showEnterpriseBadge: boolean,
   * }} accountInfo
   * @private
   */
  handleAccountInfoChanged_(accountInfo) {
    this.accountImageSrc_ = accountInfo.src;
    this.showEnterpriseBadge_ = accountInfo.showEnterpriseBadge;
  },

});
