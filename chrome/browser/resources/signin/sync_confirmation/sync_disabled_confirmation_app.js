// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './signin_shared_css.js';
import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SyncConfirmationBrowserProxy, SyncConfirmationBrowserProxyImpl} from './sync_confirmation_browser_proxy.js';


/** @polymer */
class SyncDisabledConfirmationAppElement extends PolymerElement {
  static get is() {
    return 'sync-disabled-confirmation-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!SyncConfirmationBrowserProxy} */
    this.syncConfirmationBrowserProxy_ =
        SyncConfirmationBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    document.addEventListener(
        'keydown', e => this.onKeyDown_(/** @type {!KeyboardEvent} */ (e)));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onConfirm_(e) {
    this.syncConfirmationBrowserProxy_.confirm(
        this.getConsentDescription_(),
        this.getConsentConfirmation_(e.composedPath()));
  }

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
  }

  /**
   * @return {!Array<string>} Text of the consent description elements.
   * @private
   */
  getConsentDescription_() {
    const consentDescription =
        Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  }

  /**
   * @param {!KeyboardEvent} e
   * @private
   */
  onKeyDown_(e) {
    // If the currently focused element isn't something that performs an action
    // on "enter" being pressed and the user hits "enter", perform the default
    // action of the dialog, which is "OK, Got It".
    if (e.key == 'Enter' &&
        !/^(A|PAPER-(BUTTON|CHECKBOX))$/.test(document.activeElement.tagName)) {
      this.$.confirmButton.click();
      e.preventDefault();
    }
  }

  /** @private */
  onUndo_() {
    this.syncConfirmationBrowserProxy_.undo();
  }
}

customElements.define(
    SyncDisabledConfirmationAppElement.is, SyncDisabledConfirmationAppElement);
