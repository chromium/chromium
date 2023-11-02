// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './print_preview_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './settings_section.html.js';

export class PrintPreviewSettingsSectionElement extends PolymerElement {
  static get is() {
    return 'print-preview-settings-section';
  }

  static get template() {
    return getTemplate();
  }
}

customElements.define(
    PrintPreviewSettingsSectionElement.is, PrintPreviewSettingsSectionElement);
