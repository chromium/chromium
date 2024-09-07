// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-page-content-page' contains settings related to features accessing
 * page content.
 */
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../controls/settings_toggle_button.js';
import '../settings_columned_section.css.js';
import '../settings_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './page_content_page.html.js';

export class SettingsPageContentPageElement extends PolymerElement {
  static get is() {
    return 'settings-page-content-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-page-content-page': SettingsPageContentPageElement;
  }
}

customElements.define(
    SettingsPageContentPageElement.is, SettingsPageContentPageElement);
