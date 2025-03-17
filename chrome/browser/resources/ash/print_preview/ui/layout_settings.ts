// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import './print_preview_shared.css.js';
import './settings_section.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './layout_settings.html.js';
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
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,
    };
  }

  disabled: boolean;

  static get observers() {
    return ['onLayoutSettingChange_(settings.layout.value)'];
  }

  private onLayoutSettingChange_(newValue: boolean) {
    this.selectedValue = newValue ? 'landscape' : 'portrait';
  }

  override onProcessSelectChange(value: string) {
    this.setSetting('layout', value === 'landscape');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-layout-settings': PrintPreviewLayoutSettingsElement;
  }
}

customElements.define(
    PrintPreviewLayoutSettingsElement.is, PrintPreviewLayoutSettingsElement);
