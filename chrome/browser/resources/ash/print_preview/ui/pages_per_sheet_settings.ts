// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import './print_preview_shared.css.js';
import './settings_section.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './pages_per_sheet_settings.html.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewPagesPerSheetSettingsElementBase =
    SettingsMixin(SelectMixin(PolymerElement));

export class PrintPreviewPagesPerSheetSettingsElement extends
    PrintPreviewPagesPerSheetSettingsElementBase {
  static get is() {
    return 'print-preview-pages-per-sheet-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,
    };
  }

  static get observers() {
    return [
      'onPagesPerSheetSettingChange_(settings.pagesPerSheet.value)',
    ];
  }

  disabled: boolean;

  /**
   * @param newValue The new value of the pages per sheet setting.
   */
  private onPagesPerSheetSettingChange_(newValue: number) {
    this.selectedValue = newValue.toString();
  }

  override onProcessSelectChange(value: string) {
    this.setSetting('pagesPerSheet', parseInt(value, 10));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-pages-per-sheet-settings':
        PrintPreviewPagesPerSheetSettingsElement;
  }
}

customElements.define(
    PrintPreviewPagesPerSheetSettingsElement.is,
    PrintPreviewPagesPerSheetSettingsElement);
