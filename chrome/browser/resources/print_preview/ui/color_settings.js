// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';
import {SettingsBehavior, SettingsBehaviorInterface} from './settings_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {SelectBehaviorInterface}
 * @implements {SettingsBehaviorInterface}
 */
const PrintPreviewColorSettingsElementBase =
    mixinBehaviors([SettingsBehavior, SelectBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewColorSettingsElement extends
    PrintPreviewColorSettingsElementBase {
  static get is() {
    return 'print-preview-color-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: Boolean,

      /** @private {boolean} */
      disabled_: {
        type: Boolean,
        computed: 'computeDisabled_(disabled, settings.color.setByPolicy)',
      },
    };
  }

  static get observers() {
    return ['onColorSettingChange_(settings.color.value)'];
  }

  /**
   * @param {*} newValue The new value of the color setting.
   * @private
   */
  onColorSettingChange_(newValue) {
    this.selectedValue = /** @type {boolean} */ (newValue) ? 'color' : 'bw';
  }

  /**
   * @param {boolean} disabled Whether color selection is disabled.
   * @param {boolean} managed Whether color selection is managed.
   * @return {boolean} Whether drop-down should be disabled.
   * @private
   */
  computeDisabled_(disabled, managed) {
    return !!(disabled || managed);
  }

  /** @param {string} value The new select value. */
  onProcessSelectChange(value) {
    this.setSetting('color', value === 'color');
  }
}

customElements.define(
    PrintPreviewColorSettingsElement.is, PrintPreviewColorSettingsElement);
