// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './settings_section.js';
import '/strings.m.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Settings} from '../data/model.js';

import {getCss} from './other_options_settings.css.js';
import {getHtml} from './other_options_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

interface CheckboxOption {
  name: keyof Settings;
  label: string;
  value?: boolean;
  managed?: boolean;
  available?: boolean;
}

const PrintPreviewOtherOptionsSettingsElementBase =
    SettingsMixin(I18nMixinLit(CrLitElement));

export class PrintPreviewOtherOptionsSettingsElement extends
    PrintPreviewOtherOptionsSettingsElementBase {
  static get is() {
    return 'print-preview-other-options-settings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},

      options_: {type: Array},

      /**
       * The index of the checkbox that should display the "Options" title.
       */
      firstIndex_: {type: Number},
    };
  }

  accessor disabled: boolean = false;
  protected accessor options_: CheckboxOption[] = [
    {name: 'headerFooter', label: 'optionHeaderFooter'},
    {name: 'cssBackground', label: 'optionBackgroundColorsAndImages'},
    {name: 'rasterize', label: 'optionRasterize'},
    {name: 'selectionOnly', label: 'optionSelectionOnly'},
  ];
  private accessor firstIndex_: number = 0;
  private timeouts_: Map<string, number|null> = new Map();
  private previousValues_: Map<string, boolean> = new Map();

  override connectedCallback() {
    super.connectedCallback();

    this.addSettingObserver(
        'headerFooter.*', this.onHeaderFooterSettingChange_.bind(this));
    this.onHeaderFooterSettingChange_();
    this.addSettingObserver(
        'cssBackground.*', this.onCssBackgroundSettingChange_.bind(this));
    this.onCssBackgroundSettingChange_();
    this.addSettingObserver(
        'rasterize.*', this.onRasterizeSettingChange_.bind(this));
    this.onRasterizeSettingChange_();
    this.addSettingObserver(
        'selectionOnly.*', this.onSelectionOnlySettingChange_.bind(this));
    this.onSelectionOnlySettingChange_();
  }

  /**
   * @param settingName The name of the setting to updated.
   * @param newValue The new value for the setting.
   */
  private updateSettingWithTimeout_(
      settingName: keyof Settings, newValue: boolean) {
    const timeout = this.timeouts_.get(settingName);
    if (timeout !== null) {
      clearTimeout(timeout);
    }

    this.timeouts_.set(
        settingName, setTimeout(() => {
          this.timeouts_.delete(settingName);
          if (this.previousValues_.get(settingName) === newValue) {
            return;
          }
          this.previousValues_.set(settingName, newValue);
          this.setSetting(settingName, newValue);

          // For tests only
          this.fire('update-checkbox-setting', settingName);
        }, 200));
  }

  /**
   * @param index The index of the option to update.
   */
  private updateOptionFromSetting_(index: number) {
    const setting = this.getSetting(this.options_[index]!.name);
    const option = this.options_[index]!;
    option.available = setting.available;
    option.value = setting.value;
    option.managed = setting.setByGlobalPolicy;
    this.requestUpdate();

    // Update first index
    const availableOptions = this.options_.filter(option => !!option.available);
    if (availableOptions.length > 0) {
      this.firstIndex_ = this.options_.indexOf(availableOptions[0]!);
    }
  }

  /**
   * @param managed Whether the setting is managed by policy.
   * @return Whether the checkbox should be disabled.
   */
  protected getDisabled_(managed: boolean|undefined): boolean {
    return managed || this.disabled;
  }

  private onHeaderFooterSettingChange_() {
    this.updateOptionFromSetting_(0);
  }

  private onCssBackgroundSettingChange_() {
    this.updateOptionFromSetting_(1);
  }

  private onRasterizeSettingChange_() {
    this.updateOptionFromSetting_(2);
  }

  private onSelectionOnlySettingChange_() {
    this.updateOptionFromSetting_(3);
  }

  /**
   * @param e Contains the checkbox item that was checked.
   */
  protected onChange_(e: Event) {
    const index =
        Number.parseInt((e.target as HTMLElement).dataset['index']!, 10);
    const name = this.options_[index]!.name;
    this.updateSettingWithTimeout_(
        name,
        this.shadowRoot.querySelector<CrCheckboxElement>(`#${name}`)!.checked);
  }

  /**
   * @param index The index of the settings section.
   * @return Class string containing 'first-visible' if the settings
   *     section is the first visible.
   */
  protected getClass_(index: number): string {
    return index === this.firstIndex_ ? 'first-visible' : '';
  }
}

export type OtherOptionsSettingsElement =
    PrintPreviewOtherOptionsSettingsElement;

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-other-options-settings':
        PrintPreviewOtherOptionsSettingsElement;
  }
}

customElements.define(
    PrintPreviewOtherOptionsSettingsElement.is,
    PrintPreviewOtherOptionsSettingsElement);
