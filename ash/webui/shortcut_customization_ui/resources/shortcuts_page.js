// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shortcut_input.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'shortcuts-page' is a generic page that is capable of rendering the
 * shortcuts for a specific category.
 *
 * TODO(jimmyxgong): Implement this skeleton element.
 */
export class ShortcutsPageElement extends PolymerElement {
  static get is() {
    return 'shortcuts-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(ShortcutsPageElement.is, ShortcutsPageElement);
