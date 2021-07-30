// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shortcut_input.js';
import './browser_shortcuts_page.js'
import './chromeos_shortcuts_page.js'
import './android_shortcuts_page.js'
import './accessibility_shortcuts_page.js'
import './shortcut_customization_fonts_css.js'
import './accelerator_edit_dialog.js'

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import 'chrome://resources/ash/common/navigation_view_panel.js';

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
    this.$.navigationPanel.addSelector('Chrome OS',
                                       'chromeos-shortcuts-page',
                                       'navigation-selector:laptop-chromebook');
    this.$.navigationPanel.addSelector('Browser', 'browser-shortcuts-page',
                                       'navigation-selector:laptop-chromebook');
    this.$.navigationPanel.addSelector('Android', 'android-shortcuts-page',
                                       'navigation-selector:laptop-chromebook');
    this.$.navigationPanel.addSelector('Accessibility',
                                       'accessibility-shortcuts-page',
                                       'navigation-selector:laptop-chromebook');
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

customElements.define(ShortcutCustomizationAppElement.is,
                      ShortcutCustomizationAppElement);