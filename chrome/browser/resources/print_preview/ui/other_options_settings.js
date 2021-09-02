// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';
import '../strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsBehavior, SettingsBehaviorInterface} from './settings_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {SettingsBehaviorInterface}
 */
const PrintPreviewOtherOptionsSettingsElementBase =
    mixinBehaviors([SettingsBehavior, I18nBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewOtherOptionsSettingsElement extends
    PrintPreviewOtherOptionsSettingsElementBase {
  static get is() {
    return 'print-preview-other-options-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: Boolean,

      /**
       * @private {!Array<!{name: string,
       *                    label: string,
       *                    value: (boolean | undefined),
       *                    managed: (boolean | undefined),
       *                    available: (boolean | undefined)}>}
       */
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
       * @private {number}
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

  constructor() {
    super();

    /** @private {!Map<string, ?number>} */
    this.timeouts_ = new Map();

    /** @private {!Map<string, boolean>} */
    this.previousValues_ = new Map();
  }

  /**
   * @param {string} settingName The name of the setting to updated.
   * @param {boolean} newValue The new value for the setting.
   */
  updateSettingWithTimeout_(settingName, newValue) {
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
   * @param {number} index The index of the option to update.
   * @private
   */
  updateOptionFromSetting_(index) {
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
   * @param {boolean} managed Whether the setting is managed by policy.
   * @param {boolean} disabled value of this.disabled
   * @return {boolean} Whether the checkbox should be disabled.
   * @private
   */
  getDisabled_(managed, disabled) {
    return managed || disabled;
  }

  /** @private */
  onHeaderFooterSettingChange_() {
    this.updateOptionFromSetting_(0);
  }

  /** @private */
  onCssBackgroundSettingChange_() {
    this.updateOptionFromSetting_(1);
  }

  /** @private */
  onRasterizeSettingChange_() {
    this.updateOptionFromSetting_(2);
  }

  /** @private */
  onSelectionOnlySettingChange_() {
    this.updateOptionFromSetting_(3);
  }

  /**
   * @param {!Event} e Contains the checkbox item that was checked.
   * @private
   */
  onChange_(e) {
    const name = e.model.item.name;
    this.updateSettingWithTimeout_(
        name, this.shadowRoot.querySelector(`#${name}`).checked);
  }

  /**
   * @param {number} index The index of the settings section.
   * @return {string} Class string containing 'first-visible' if the settings
   *     section is the first visible.
   * @private
   */
  getClass_(index) {
    return index === this.firstIndex_ ? 'first-visible' : '';
  }
}

customElements.define(
    PrintPreviewOtherOptionsSettingsElement.is,
    PrintPreviewOtherOptionsSettingsElement);
