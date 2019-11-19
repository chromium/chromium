/* Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import './strings.m.js';
import './signin_shared_css.js';

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
      value: function() {
        return loadTimeData.getString('accountPictureUrl');
      },
    },
  },

  /** @private {?SyncConfirmationBrowserProxy} */
  syncConfirmationBrowserProxy_: null,

  /** @private {?function(Event)} */
  boundKeyDownHandler_: null,

  /** @override */
  attached: function() {
    this.syncConfirmationBrowserProxy_ =
        SyncConfirmationBrowserProxyImpl.getInstance();
    this.boundKeyDownHandler_ = this.onKeyDown_.bind(this);
    // This needs to be bound to document instead of "this" because the dialog
    // window opens initially, the focus level is only on document, so the key
    // event is not captured by "this".
    document.addEventListener('keydown', this.boundKeyDownHandler_);
    this.addWebUIListener(
        'account-image-changed', this.handleAccountImageChanged_.bind(this));
    this.syncConfirmationBrowserProxy_.requestAccountImage();
  },

  /** @override */
  detached: function() {
    document.removeEventListener('keydown', this.boundKeyDownHandler_);
  },

  /** @private */
  onConfirm_: function(e) {
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path));
  },

  /** @private */
  onUndo_: function() {
    this.syncConfirmationBrowserProxy_.undo();
  },

  /** @private */
  onGoToSettings_: function(e) {
    this.syncConfirmationBrowserProxy_.goToSettings(
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path));
  },

  /** @private */
  onKeyDown_: function(e) {
    if (e.key == 'Enter' && !/^(A|CR-BUTTON)$/.test(e.path[0].tagName)) {
      this.onConfirm_(e);
      e.preventDefault();
    }
  },

  /**
   * @param {!Array<!HTMLElement>} path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return {string} The text of the consent confirmation element.
   * @private
   */
  getConsentConfirmation_: function(path) {
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
  getConsentDescription_: function() {
    const consentDescription =
        Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  },

  /**
   * Called when the account image changes.
   * @param {string} imageSrc
   * @private
   */
  handleAccountImageChanged_: function(imageSrc) {
    this.accountImageSrc_ = imageSrc;
  },

});
