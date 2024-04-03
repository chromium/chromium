// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './print_preview_shared.css.js';
import './settings_section.js';
import '../strings.m.js';

import type {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {Settings} from '../data/model.js';

import {getTemplate} from './other_options_settings.html.js';
import {SettingsMixin} from './settings_mixin.js';

interface CheckboxOption {
  name: keyof Settings;
  label: string;
  value?: boolean;
  managed?: boolean;
  available?: boolean;
}

const PrintPreviewOtherOptionsSettingsElementBase =
    SettingsMixin(I18nMixin(PolymerElement));

export class PrintPreviewOtherOptionsSettingsElement extends
    PrintPreviewOtherOptionsSettingsElementBase {
  static get is() {
    return 'print-preview-other-options-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      disabled: Boolean,

      options_: {
        type: Array,
        value() {
          return [
            {name: 'headerFooter', label: 'optionHeaderFooter'},
            {name: 'cssBackground', label: 'optionBackgroundColorsAndImages'},
            {name: 'rasterize', label: 'optionRasterize'},
            {name: 'selectionOnly', label: 'optionSelectionOnly'},
          ];
        },
      },

      /**
       * The index of the checkbox that should display the "Options" title.
       */
      firstIndex_: {
        type: Number,
        value: 0,
      },
    };
  }

  static get observers() {
    return [
      'onHeaderFooterSettingChange_(settings.headerFooter.*)',
      'onCssBackgroundSettingChange_(settings.cssBackground.*)',
      'onRasterizeSettingChange_(settings.rasterize.*)',
      'onSelectionOnlySettingChange_(settings.selectionOnly.*)',
    ];
  }

  disabled: boolean;
  private options_: CheckboxOption[];
  private firstIndex_: number;
  private timeouts_: Map<string, number|null> = new Map();
  private previousValues_: Map<string, boolean> = new Map();

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
          this.dispatchEvent(new CustomEvent(
              'update-checkbox-setting',
              {bubbles: true, composed: true, detail: settingName}));
        }, 200));
  }

  /**
   * @param index The index of the option to update.
   */
  private updateOptionFromSetting_(index: number) {
    const setting = this.getSetting(this.options_[index].name);
    this.set(`options_.${index}.available`, setting.available);
    this.set(`options_.${index}.value`, setting.value);
    this.set(`options_.${index}.managed`, setting.setByPolicy);

    // Update first index
    const availableOptions = this.options_.filter(option => !!option.available);
    if (availableOptions.length > 0) {
      this.firstIndex_ = this.options_.indexOf(availableOptions[0]);
    }
  }

  /**
   * @param managed Whether the setting is managed by policy.
   * @param disabled value of this.disabled
   * @return Whether the checkbox should be disabled.
   */
  private getDisabled_(managed: boolean, disabled: boolean): boolean {
    return managed || disabled;
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
  private onChange_(e: DomRepeatEvent<CheckboxOption>) {
    const name = e.model.item.name;
    this.updateSettingWithTimeout_(
        name,
        this.shadowRoot!.querySelector<CrCheckboxElement>(`#${name}`)!.checked);
  }

  /**
   * @param index The index of the settings section.
   * @return Class string containing 'first-visible' if the settings
   *     section is the first visible.
   */
  private getClass_(index: number): string {
    return index === this.firstIndex_ ? 'first-visible' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-other-options-settings':
        PrintPreviewOtherOptionsSettingsElement;
  }
}

customElements.define(
    PrintPreviewOtherOptionsSettingsElement.is,
    PrintPreviewOtherOptionsSettingsElement);
