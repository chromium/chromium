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
 * @implements {SettingsBehaviorInterface}
 * @implements {SelectBehaviorInterface}
 */
const PrintPreviewLayoutSettingsElementBase =
    mixinBehaviors([SettingsBehavior, SelectBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewLayoutSettingsElement extends
    PrintPreviewLayoutSettingsElementBase {
  static get is() {
    return 'print-preview-layout-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: Boolean,
    };
  }

  static get observers() {
    return ['onLayoutSettingChange_(settings.layout.value)'];
  }

  /**
   * @param {*} newValue The new value of the layout setting.
   * @private
   */
  onLayoutSettingChange_(newValue) {
    this.selectedValue =
        /** @type {boolean} */ (newValue) ? 'landscape' : 'portrait';
  }

  /** @param {string} value The new select value. */
  onProcessSelectChange(value) {
    this.setSetting('layout', value === 'landscape');
  }
}

customElements.define(
    PrintPreviewLayoutSettingsElement.is, PrintPreviewLayoutSettingsElement);
