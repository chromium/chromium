// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MarginsType} from '../data/margins.js';

import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';
import {SettingsBehavior, SettingsBehaviorInterface} from './settings_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {SelectBehaviorInterface}
 * @implements {SettingsBehaviorInterface}
 */
const PrintPreviewPagesPerSheetSettingsElementBase =
    mixinBehaviors([SettingsBehavior, SelectBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewPagesPerSheetSettingsElement extends
    PrintPreviewPagesPerSheetSettingsElementBase {
  static get is() {
    return 'print-preview-pages-per-sheet-settings';
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
    return [
      'onPagesPerSheetSettingChange_(settings.pagesPerSheet.value)',
    ];
  }

  /**
   * @param {*} newValue The new value of the pages per sheet setting.
   * @private
   */
  onPagesPerSheetSettingChange_(newValue) {
    this.selectedValue = /** @type {number} */ (newValue).toString();
  }

  /** @param {string} value The new select value. */
  onProcessSelectChange(value) {
    this.setSetting('pagesPerSheet', parseInt(value, 10));
  }
}

customElements.define(
    PrintPreviewPagesPerSheetSettingsElement.is,
    PrintPreviewPagesPerSheetSettingsElement);
