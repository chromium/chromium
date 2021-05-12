// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'keyboard-shortcut-banner' is an element to display reminders
 * for keyboard shortcuts.
 *
 * The body of the reminder should be specified as an array of strings given by
 * the "body" property. Use a <kbd> element to wrap entire shortcuts, and use
 * nested <kbd> elements to signify keys. Do not add spaces around the + between
 * keyboard shortcuts. For example, "Press Ctrl + Space" should be passed in as
 * "Press <kbd><kbd>Ctrl</kbd>+<kbd>Space</kbd></kbd>".
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class KeyboardShortcutBanner extends PolymerElement {
  static get is() {
    return 'keyboard-shortcut-banner';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      title: {
        type: String,
      },

      /** @type {!Array<string>} */
      body: {
        type: Array,
      }
    };
  }

  /** @private */
  onDismissClick_() {
    this.dispatchEvent(new CustomEvent('dismiss'));
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getIdForIndex_(index) {
    return `id${index}`;
  }

  /**
   * @return {string}
   * @private
   */
  getBodyIds_() {
    const /** !Array<string> */ ids = [];
    for (let i = 0; i < this.body.length; i++) {
      ids.push(this.getIdForIndex_(i));
    }
    return ids.join(' ');
  }
}

customElements.define(KeyboardShortcutBanner.is, KeyboardShortcutBanner);
