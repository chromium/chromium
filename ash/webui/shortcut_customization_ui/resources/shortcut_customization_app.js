// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './accelerator_edit_dialog.js'
import './shortcut_input.js';
import './shortcuts_page.js'
import './shortcut_customization_fonts_css.js'
import 'chrome://resources/ash/common/navigation_view_panel.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'shortcut-customization-app' is the main landing page for the shortcut
 * customization app.
 */
export class ShortcutCustomizationAppElement extends PolymerElement {
  static get is() {
    return 'shortcut-customization-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @private */
      dialogShortcutTitle_: {
        type: String,
        value: '',
      },

      /**
       * @type {!Array<!Object>}
       * @private
       */
      dialogAccelerators_: {
        type: Array,
        value: () => {},
      },

      /** @private */
      showEditDialog_: {
        type: Boolean,
        value: false,
      },
    }
  }

  ready() {
    super.ready();
    this.$.navigationPanel.addSelector(
        'Chrome OS', 'shortcuts-page', 'navigation-selector:laptop-chromebook',
        'chromeos-page-id');
    this.$.navigationPanel.addSelector(
        'Browser', 'shortcuts-page', 'navigation-selector:laptop-chromebook',
        'browser-page-id');
    this.$.navigationPanel.addSelector(
        'Android', 'shortcuts-page', 'navigation-selector:laptop-chromebook',
        'android-page-id');
    this.$.navigationPanel.addSelector(
        'Accessibility', 'shortcuts-page',
        'navigation-selector:laptop-chromebook', 'a11y-page-id');
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    window.addEventListener('show-edit-dialog',
        (e) => this.showDialog_(e.detail));
    window.addEventListener('edit-dialog-closed', () => this.onDialogClosed_());
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('show-edit-dialog',
        (e) => this.showDialog_(e.detail));
    window.removeEventListener('edit-dialog-closed',
        () => this.onDialogClosed_());
  }

  /**
   * @param {!{description: string, accelerators: !Array<!Object>}} e
   * @private
   */
  showDialog_(e) {
    this.dialogShortcutTitle_ = e.description;
    this.dialogAccelerators_ = /** @type {!Array<!Object>}*/(e.accelerators);
    this.showEditDialog_ = true;
  }

  /** @private */
  onDialogClosed_() {
    this.showEditDialog_ = false;
    this.dialogShortcutTitle_ = '';
    this.dialogAccelerators_ = [];
  }
}

customElements.define(
    ShortcutCustomizationAppElement.is, ShortcutCustomizationAppElement);
