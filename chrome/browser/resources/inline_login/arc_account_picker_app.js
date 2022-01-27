// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './account_manager_shared_css.js';

import {getImage} from '//resources/js/icon.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {Account} from './inline_login_browser_proxy.js';
import {InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';

/** @polymer */
class ArcAccountPickerAppElement extends PolymerElement {
  static get is() {
    return 'arc-account-picker-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Accounts which are not available in ARC and are shown on the ARC picker
       * screen.
       * @type {!Array<!Account>}
       */
      accounts: {
        type: Array,
      }
    };
  }

  ready() {
    super.ready();

    this.shadowRoot.querySelector('#osSettingsLink')
        .addEventListener(
            'click',
            () => this.dispatchEvent(new CustomEvent('opened-new-window')));
  }

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return getImage(iconUrl);
  }

  /**
   * Navigates to the welcome screen.
   * @private
   */
  addAccount_() {
    this.dispatchEvent(new CustomEvent('add-account'));
  }

  /**
   * @param {!{model: !{item: Account}, target: !Element}} event
   * @private
   */
  makeAvailableInArc_(event) {
    InlineLoginBrowserProxyImpl.getInstance().makeAvailableInArc(
        event.model.item);
  }
}

customElements.define(
    ArcAccountPickerAppElement.is, ArcAccountPickerAppElement);
