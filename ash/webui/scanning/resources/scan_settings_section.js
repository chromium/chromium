// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_fonts_css.js';
import './scanning_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class ScanSettingsSectionElement extends PolymerElement {
  static get is() {
    return 'scan-settings-section';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    ScanSettingsSectionElement.is, ScanSettingsSectionElement);
