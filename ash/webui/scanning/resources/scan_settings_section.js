// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './scanning_fonts_css.js';
import './scanning_shared_css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './scan_settings_section.html.js';

/** @polymer */
class ScanSettingsSectionElement extends PolymerElement {
  static get is() {
    return 'scan-settings-section';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    ScanSettingsSectionElement.is, ScanSettingsSectionElement);
