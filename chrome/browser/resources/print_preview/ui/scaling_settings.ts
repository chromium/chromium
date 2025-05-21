// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import './number_settings_section.js';
import './settings_section.js';

import {getCss as getMdSelectLitCss} from 'chrome://resources/cr_elements/md_select_lit.css.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Settings} from '../data/model.js';
import {ScalingType} from '../data/scaling.js';

import {getCss as getPrintPreviewSharedCss} from './print_preview_shared.css.js';
import {getHtml} from './scaling_settings.html.js';
import {SelectMixin} from './select_mixin.js';
import {SettingsMixin} from './settings_mixin.js';

/*
 * Fit to page and fit to paper options will only be displayed for PDF
 * documents. If the custom option is selected, an additional input field will
 * appear to enter the custom scale factor.
 */

const PrintPreviewScalingSettingsElementBase =
    SettingsMixin(SelectMixin(CrLitElement));

export class PrintPreviewScalingSettingsElement extends
    PrintPreviewScalingSettingsElementBase {
  static get is() {
    return 'print-preview-scaling-settings';
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
      isPdf: {type: Boolean},
      currentValue_: {type: String},
      customSelected_: {type: Boolean},
      scalingTypeValue_: {type: Number},
      scalingTypePdfValue_: {type: Number},
      inputValid_: {type: Boolean},
      dropdownDisabled_: {type: Boolean},
      settingKey_: {type: String},
    };
  }

  accessor disabled: boolean = false;
  accessor isPdf: boolean = false;
  protected accessor currentValue_: string = '';
  protected accessor customSelected_: boolean = false;
  protected accessor dropdownDisabled_: boolean = false;
  protected accessor inputValid_: boolean = false;
  private accessor settingKey_: keyof Settings = 'scalingType';
  private accessor scalingTypeValue_: ScalingType = ScalingType.DEFAULT;
  private accessor scalingTypePdfValue_: ScalingType = ScalingType.DEFAULT;

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

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver(
        'scaling.value', this.onScalingSettingChanged_.bind(this));
    this.onScalingSettingChanged_();

    this.addSettingObserver('scalingType.*', () => {
      this.scalingTypeValue_ = this.getSettingValue('scalingType');
    });
    this.addSettingObserver('scalingTypePdf.*', () => {
      this.scalingTypePdfValue_ = this.getSettingValue('scalingTypePdf');
    });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedProperties.has('isPdf')) {
      this.settingKey_ = this.isPdf ? 'scalingTypePdf' : 'scalingType';
    }

    if (changedPrivateProperties.has('settingKey_') ||
        changedPrivateProperties.has('scalingTypeValue_') ||
        changedPrivateProperties.has('scalingTypePdfValue_')) {
      this.customSelected_ = this.computeCustomSelected_();
      this.onScalingTypeSettingChanged_();
    }

    if (changedProperties.has('disabled') ||
        changedPrivateProperties.has('inputValid_')) {
      this.dropdownDisabled_ = this.disabled && this.inputValid_;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('inputValid_') ||
        changedPrivateProperties.has('currentValue_')) {
      this.onInputFieldChanged_();
    }
  }


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

  /**
   * @return Whether the input should be disabled.
   */
  protected inputDisabled_(): boolean {
    return !this.customSelected_ || this.dropdownDisabled_;
  }

  /**
   * @return Whether the custom scaling option is selected.
   */
  private computeCustomSelected_(): boolean {
    return !!this.settingKey_ &&
        this.getSettingValue(this.settingKey_) === ScalingType.CUSTOM;
  }

  protected onCollapseChanged_() {
    if (this.customSelected_ && this.userSelectedCustomScaling_) {
      this.shadowRoot.querySelector('print-preview-number-settings-section')!
          .getInput()
          .focus();
    }
    this.customScalingSettingSet_ = false;
    this.userSelectedCustomScaling_ = false;
  }

  protected onCurrentValueChanged_(e: CustomEvent<{value: string}>) {
    this.currentValue_ = e.detail.value;
  }

  protected onInputValidChanged_(e: CustomEvent<{value: boolean}>) {
    this.inputValid_ = e.detail.value;
  }

  protected isSelected_(value: ScalingType): boolean {
    return this.selectedValue === value.toString();
  }
}

export type ScalingSettingsElement = PrintPreviewScalingSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-scaling-settings': PrintPreviewScalingSettingsElement;
  }
}

customElements.define(
    PrintPreviewScalingSettingsElement.is, PrintPreviewScalingSettingsElement);
