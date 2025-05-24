// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './settings_section.js';

import {getCss as getMdSelectLitCss} from 'chrome://resources/cr_elements/md_select_lit.css.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './layout_settings.html.js';
import {getCss as getPrintPreviewSharedCss} from './print_preview_shared.css.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewLayoutSettingsElementBase =
    SettingsMixin(SelectMixin(CrLitElement));

export class PrintPreviewLayoutSettingsElement extends
    PrintPreviewLayoutSettingsElementBase {
  static get is() {
    return 'print-preview-layout-settings';
  }

  static override get styles() {
    return [
      getPrintPreviewSharedCss(),
      getMdSelectLitCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
    };
  }

  accessor disabled: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.onLayoutSettingChange_(this.getSettingValue('layout'));
    this.addSettingObserver('layout.value', (newValue: boolean) => {
      this.onLayoutSettingChange_(newValue);
    });
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

export type LayoutSettingsElement = PrintPreviewLayoutSettingsElement;

customElements.define(
    PrintPreviewLayoutSettingsElement.is, PrintPreviewLayoutSettingsElement);
