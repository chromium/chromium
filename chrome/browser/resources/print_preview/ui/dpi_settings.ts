// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './print_preview_shared.css.js';
import './settings_section.js';
import '../strings.m.js';
import './settings_select.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {DpiCapability, DpiOption, SelectOption} from '../data/cdd.js';

import {getTemplate} from './dpi_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

type LabelledDpiOption = DpiOption&SelectOption;
export interface LabelledDpiCapability {
  option: LabelledDpiOption[];
}

const PrintPreviewDpiSettingsElementBase = SettingsMixin(PolymerElement);

export class PrintPreviewDpiSettingsElement extends
    PrintPreviewDpiSettingsElementBase {
  static get is() {
    return 'print-preview-dpi-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      capability: Object,

      disabled: Boolean,

      capabilityWithLabels_: {
        type: Object,
        computed: 'computeCapabilityWithLabels_(capability)',
      },
    };
  }

  static get observers() {
    return [
      'onDpiSettingChange_(settings.dpi.*, capabilityWithLabels_.option)',
    ];
  }

  capability: DpiCapability;
  disabled: boolean;
  private capabilityWithLabels_: DpiCapability;

  /**
   * Adds default labels for each option.
   */
  private computeCapabilityWithLabels_(): LabelledDpiCapability|null {
    if (this.capability === undefined) {
      return null;
    }

    const result: LabelledDpiCapability = structuredClone(this.capability);
    this.capability.option.forEach((dpiOption, index) => {
      const hDpi = dpiOption.horizontal_dpi || 0;
      const vDpi = dpiOption.vertical_dpi || 0;
      if (hDpi > 0 && vDpi > 0 && hDpi !== vDpi) {
        result.option[index].name = loadTimeData.getStringF(
            'nonIsotropicDpiItemLabel', hDpi.toLocaleString(),
            vDpi.toLocaleString());
      } else {
        result.option[index].name = loadTimeData.getStringF(
            'dpiItemLabel', (hDpi || vDpi).toLocaleString());
      }
    });
    return result;
  }

  private onDpiSettingChange_() {
    if (this.capabilityWithLabels_ === null ||
        this.capabilityWithLabels_ === undefined) {
      return;
    }

    const dpiValue = this.getSettingValue('dpi') as DpiOption;
    for (const option of this.capabilityWithLabels_.option) {
      const dpiOption = option as LabelledDpiOption;
      if (dpiValue.horizontal_dpi === dpiOption.horizontal_dpi &&
          dpiValue.vertical_dpi === dpiOption.vertical_dpi &&
          dpiValue.vendor_id === dpiOption.vendor_id) {
        this.shadowRoot!.querySelector('print-preview-settings-select')!
            .selectValue(JSON.stringify(option));
        return;
      }
    }

    const defaultOption =
        this.capabilityWithLabels_.option.find(o => !!o.is_default) ||
        this.capabilityWithLabels_.option[0];
    this.setSetting('dpi', defaultOption);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-dpi-settings': PrintPreviewDpiSettingsElement;
  }
}

customElements.define(
    PrintPreviewDpiSettingsElement.is, PrintPreviewDpiSettingsElement);
