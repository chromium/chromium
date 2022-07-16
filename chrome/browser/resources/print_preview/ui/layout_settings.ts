// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewLayoutSettingsElementBase =
    SettingsMixin(SelectMixin(PolymerElement));

export class PrintPreviewLayoutSettingsElement extends
    PrintPreviewLayoutSettingsElementBase {
  static get is() {
    return 'print-preview-layout-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: Boolean,
    };
  }

  static get observers() {
    return ['onLayoutSettingChange_(settings.layout.value)'];
  }

  private onLayoutSettingChange_(newValue: boolean) {
    this.selectedValue = newValue ? 'landscape' : 'portrait';
  }

  onProcessSelectChange(value: string) {
    this.setSetting('layout', value === 'landscape');
  }
}

customElements.define(
    PrintPreviewLayoutSettingsElement.is, PrintPreviewLayoutSettingsElement);
