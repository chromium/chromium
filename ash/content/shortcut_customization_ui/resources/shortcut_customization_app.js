// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shortcut_input.js';
import './browser_shortcuts_page.js'
import './chromeos_shortcuts_page.js'

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

  ready() {
    super.ready();
    this.$.navigationPanel.addSelector('Chrome OS',
                                       'chromeos-shortcuts-page');
    this.$.navigationPanel.addSelector('Browser', 'browser-shortcuts-page');
  }
}

customElements.define(ShortcutCustomizationAppElement.is,
                      ShortcutCustomizationAppElement);