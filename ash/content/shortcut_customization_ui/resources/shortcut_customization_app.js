// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shortcut_input.js';

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

  ready() {
    super.ready();
    // TODO(jimmyxgong): Remove this once the app has more capabilities.
    this.$.header.textContent = 'Shortcut Customization';
  }
}

customElements.define(ShortcutCustomizationAppElement.is,
                      ShortcutCustomizationAppElement);