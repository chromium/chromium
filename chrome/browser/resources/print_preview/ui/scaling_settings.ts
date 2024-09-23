// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import './number_settings_section.js';
import './print_preview_shared.css.js';
import './settings_section.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Settings} from '../data/model.js';
import {ScalingType} from '../data/scaling.js';

import {getTemplate} from './scaling_settings.html.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

/*
 * Fit to page and fit to paper options will only be displayed for PDF
 * documents. If the custom option is selected, an additional input field will
 * appear to enter the custom scale factor.
 */

const PrintPreviewScalingSettingsElementBase =
    SettingsMixin(SelectMixin(PolymerElement));

export class PrintPreviewScalingSettingsElement extends
    PrintPreviewScalingSettingsElementBase {
  static get is() {
    return 'print-preview-scaling-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        observer: 'onDisabledChanged_',
      },

      isPdf: Boolean,

      currentValue_: {
        type: String,
      },

      customSelected_: {
        type: Boolean,
        computed: 'computeCustomSelected_(settingKey_, ' +
            'settings.scalingType.*, settings.scalingTypePdf.*)',
      },

      inputValid_: Boolean,

      dropdownDisabled_: {
        type: Boolean,
        value: false,
      },

      settingKey_: {
        type: String,
        computed: 'computeSettingKey_(isPdf)',
      },

      /** Mirroring the enum so that it can be used from HTML bindings. */
      ScalingValue: {
        type: Object,
        value: ScalingType,
      },
    };
  }

  static get observers() {
    return [
      'onScalingTypeSettingChanged_(settingKey_, settings.scalingType.value, ' +
          'settings.scalingTypePdf.value)',
      'onScalingSettingChanged_(settings.scaling.value)',
      'onInputFieldChanged_(inputValid_, currentValue_)',
    ];
  }

  disabled: boolean;
  isPdf: boolean;
  private currentValue_: string;
  private customSelected_: boolean;
  private dropdownDisabled_: boolean;
  private inputValid_: boolean;
  private settingKey_: keyof Settings;

  private lastValidScaling_: string = '';

  /**
   * Whether the custom scaling setting has been set to true, but the custom
   * input has not yet been expanded. Used to determine whether changes in the
   * dropdown are due to user input or sticky settings.
   */
  private customScalingSettingSet_: boolean = false;

  /**
   * Whether the user has selected custom scaling in the dropdown, but the
   * custom input has not yet been expanded. Used to determine whether to
   * auto-focus the custom input.
   */
  private userSelectedCustomScaling_: boolean = false;

  override onProcessSelectChange(value: string) {
    const isCustom = value === ScalingType.CUSTOM.toString();
    if (isCustom && !this.customScalingSettingSet_) {
      this.userSelectedCustomScaling_ = true;
    } else {
      this.customScalingSettingSet_ = false;
    }

    const valueAsNumber = parseInt(value, 10);
    if (isCustom || value === ScalingType.DEFAULT.toString()) {
      this.setSetting('scalingType', valueAsNumber);
    }
    if (this.isPdf ||
        this.getSetting('scalingTypePdf').value === ScalingType.DEFAULT ||
        this.getSetting('scalingTypePdf').value === ScalingType.CUSTOM) {
      this.setSetting('scalingTypePdf', valueAsNumber);
    }

    if (isCustom) {
      this.setSetting('scaling', this.currentValue_);
    }
  }

  private updateScalingToValid_() {
    if (!this.getSetting('scaling').valid) {
      this.currentValue_ = this.lastValidScaling_;
    } else {
      this.lastValidScaling_ = this.currentValue_;
    }
  }

  /**
   * Updates the input string when scaling setting is set.
   */
  private onScalingSettingChanged_() {
    const value = this.getSetting('scaling').value as string;
    this.lastValidScaling_ = value;
    this.currentValue_ = value;
  }

  private onScalingTypeSettingChanged_() {
    if (!this.settingKey_) {
      return;
    }

    const value = this.getSettingValue(this.settingKey_) as ScalingType;
    if (value !== ScalingType.CUSTOM) {
      this.updateScalingToValid_();
    } else {
      this.customScalingSettingSet_ = true;
    }
    this.selectedValue = value.toString();
  }

  /**
   * Updates scaling settings based on the validity and current value of the
   * scaling input.
   */
  private onInputFieldChanged_() {
    this.setSettingValid('scaling', this.inputValid_);

    if (this.currentValue_ !== undefined && this.currentValue_ !== '' &&
        this.inputValid_ &&
        this.currentValue_ !== this.getSettingValue('scaling')) {
      this.setSetting('scaling', this.currentValue_);
    }
  }

  private onDisabledChanged_() {
    this.dropdownDisabled_ = this.disabled && this.inputValid_;
  }

  /**
   * @return Whether the input should be disabled.
   */
  private inputDisabled_(): boolean {
    return !this.customSelected_ || this.dropdownDisabled_;
  }

  /**
   * @return Whether the custom scaling option is selected.
   */
  private computeCustomSelected_(): boolean {
    return !!this.settingKey_ &&
        this.getSettingValue(this.settingKey_) === ScalingType.CUSTOM;
  }

  /**
   * @return The key of the appropriate scaling setting.
   */
  private computeSettingKey_(): string {
    return this.isPdf ? 'scalingTypePdf' : 'scalingType';
  }

  private onCollapseChanged_() {
    if (this.customSelected_ && this.userSelectedCustomScaling_) {
      this.shadowRoot!.querySelector('print-preview-number-settings-section')!
          .getInput()
          .focus();
    }
    this.customScalingSettingSet_ = false;
    this.userSelectedCustomScaling_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-scaling-settings': PrintPreviewScalingSettingsElement;
  }
}

customElements.define(
    PrintPreviewScalingSettingsElement.is, PrintPreviewScalingSettingsElement);
