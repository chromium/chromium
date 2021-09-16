// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pdf-documents' is the polymer element for showing the
 * settings for viewing PDF documents under Site Settings.
 */

import '../controls/settings_toggle_button.js';
import '../settings_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

class SettingsPdfDocumentsElement extends PolymerElement {
  static get is() {
    return 'settings-pdf-documents';
  }

  static get template() {
    return html`{__html_template__}`;
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

customElements.define(
    SettingsPdfDocumentsElement.is, SettingsPdfDocumentsElement);
