// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_lit.css.js';
import './print_preview_shared.css.js';
import './settings_section.js';

import {getCss as getMdSelectLitCss} from 'chrome://resources/cr_elements/md_select_lit.css.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getHtml} from './color_settings.html.js';
import {getCss as getPrintPreviewSharedCss} from './print_preview_shared.css.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

const PrintPreviewColorSettingsElementBase =
    SettingsMixin(SelectMixin(CrLitElement));

export class PrintPreviewColorSettingsElement extends
    PrintPreviewColorSettingsElementBase {
  static get is() {
    return 'print-preview-color-settings';
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
      managed_: {type: Boolean},
    };
  }

  accessor disabled: boolean = false;
  private accessor managed_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver(
        'color.value', this.onColorSettingChange_.bind(this));
    this.onColorSettingChange_(this.getSettingValue('color'));

    this.addSettingObserver('color.setByGlobalPolicy', (value: boolean) => {
      this.managed_ = value;
    });
    this.managed_ = this.getSetting('color').setByGlobalPolicy;
  }

  private onColorSettingChange_(newValue: boolean) {
    this.selectedValue = newValue ? 'color' : 'bw';
  }

  /**
   * @return Whether drop-down should be disabled.
   */
  protected computeDisabled_(): boolean {
    return this.disabled || this.managed_;
  }

  /** @param value The new select value. */
  override onProcessSelectChange(value: string) {
    this.setSetting('color', value === 'color');
  }
}

export type ColorSettingsElement = PrintPreviewColorSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-color-settings': PrintPreviewColorSettingsElement;
  }
}

customElements.define(
    PrintPreviewColorSettingsElement.is, PrintPreviewColorSettingsElement);
