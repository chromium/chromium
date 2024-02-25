// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_fonts.css.js';
import './scanning_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './scan_settings_section.html.js';

class ScanSettingsSectionElement extends PolymerElement {
  static get is() {
    return 'scan-settings-section' as const;
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ScanSettingsSectionElement.is]: ScanSettingsSectionElement;
  }
}

customElements.define(
    ScanSettingsSectionElement.is, ScanSettingsSectionElement);
