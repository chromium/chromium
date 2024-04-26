// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './print_preview_shared.css.js';
import './settings_section.js';
import './settings_select.js';

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MediaSizeCapability} from '../data/cdd.js';

import {getTemplate} from './media_size_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

export interface PrintPreviewMediaSizeSettingsElement {
  $: {
    borderless: CrCheckboxElement,
  };
}

const PrintPreviewMediaSizeSettingsElementBase = SettingsMixin(PolymerElement);

export class PrintPreviewMediaSizeSettingsElement extends
    PrintPreviewMediaSizeSettingsElementBase {
  static get is() {
    return 'print-preview-media-size-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      capability: Object,

      disableBorderlessCheckbox_: {
        type: Boolean,
        computed: 'computeDisableBorderlessCheckbox_(disabled, ' +
            'settings.mediaSize.value.has_borderless_variant)',
      },

      disabled: Boolean,
    };
  }

  capability: MediaSizeCapability;
  disabled: boolean;
  private disableBorderlessCheckbox_: boolean;

  static get observers() {
    return [
      'onMediaSizeSettingChange_(settings.mediaSize.*, capability.option)',
      'updateBorderlessAvailabilityForSize_(' +
          'settings.mediaSize.*, settings.borderless.*)',
    ];
  }

  private onMediaSizeSettingChange_() {
    if (!this.capability) {
      return;
    }
    const valueToSet = JSON.stringify(this.getSettingValue('mediaSize'));
    for (const option of this.capability.option) {
      if (JSON.stringify(option) === valueToSet) {
        this.shadowRoot!.querySelector('print-preview-settings-select')!
            .selectValue(valueToSet);
        return;
      }
    }

    const defaultOption = this.capability.option.find(o => !!o.is_default) ||
        this.capability.option[0];
    this.setSetting('mediaSize', defaultOption);
  }

  private computeDisableBorderlessCheckbox_(
      disabled: boolean, hasBorderlessVariant: boolean): boolean {
    return disabled || !hasBorderlessVariant;
  }

  private updateBorderlessAvailabilityForSize_() {
    if (!loadTimeData.getBoolean('isBorderlessPrintingEnabled')) {
      return;
    }
    const size = this.getSettingValue('mediaSize');
    if (size.has_borderless_variant) {
      this.$.borderless.checked = this.getSettingValue('borderless');
    } else {
      // If a size only supports borderless and has no bordered variant,
      // has_borderless_variant will be false. In this case, the checkbox
      // will be disabled (it wouldn't have any effect), but will display
      // as checked to indicate that the print will be borderless. This is
      // a corner case, but printers are allowed to do it, so it's best to
      // handle it as well as possible. If a size only supports bordered and
      // not borderless, disable the checkbox and leave it unchecked.
      this.$.borderless.checked =
          (size?.imageable_area_left_microns === 0 &&
           size?.imageable_area_bottom_microns === 0 &&
           size?.imageable_area_right_microns === size.width_microns &&
           size?.imageable_area_top_microns === size.height_microns);
    }
  }

  private onBorderlessCheckboxChange_() {
    this.setSetting('borderless', this.$.borderless.checked);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-media-size-settings': PrintPreviewMediaSizeSettingsElement;
  }
}

customElements.define(
    PrintPreviewMediaSizeSettingsElement.is,
    PrintPreviewMediaSizeSettingsElement);
