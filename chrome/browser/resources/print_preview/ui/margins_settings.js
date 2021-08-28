// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/md_select_css.m.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MarginsType} from '../data/margins.js';
import {State} from '../data/state.js';

import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';
import {SettingsBehavior, SettingsBehaviorInterface} from './settings_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {SelectBehaviorInterface}
 * @implements {SettingsBehaviorInterface}
 */
const PrintPreviewMarginsSettingsElementBase =
    mixinBehaviors([SettingsBehavior, SelectBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewMarginsSettingsElement extends
    PrintPreviewMarginsSettingsElementBase {
  static get is() {
    return 'print-preview-margins-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      disabled: {
        type: Boolean,
        observer: 'updateMarginsDisabled_',
      },

      /** @type {!State} */
      state: {
        type: Number,
        observer: 'onStateChange_',
      },

      /** @private */
      marginsDisabled_: Boolean,

      /** Mirroring the enum so that it can be used from HTML bindings. */
      MarginsTypeEnum: Object,
    };
  }

  static get observers() {
    return [
      'onMarginsSettingChange_(settings.margins.value)',
      'onMediaSizeOrLayoutChange_(' +
          'settings.mediaSize.value, settings.layout.value)',
      'onPagesPerSheetSettingChange_(settings.pagesPerSheet.value)'

    ];
  }

  constructor() {
    super();

    /** @private {boolean} */
    this.loaded_ = false;
  }

  /** @override */
  ready() {
    super.ready();

    this.MarginsTypeEnum = MarginsType;
  }

  /** @private */
  onStateChange_() {
    if (this.state === State.READY) {
      this.loaded_ = true;
    }
  }

  /** @private */
  onMediaSizeOrLayoutChange_() {
    if (this.loaded_ &&
        this.getSetting('margins').value === MarginsType.CUSTOM) {
      this.setSetting('margins', MarginsType.DEFAULT);
    }
  }

  /**
   * @param {number} newValue The new value of the pages per sheet setting.
   * @private
   */
  onPagesPerSheetSettingChange_(newValue) {
    if (newValue > 1) {
      this.setSetting('margins', MarginsType.DEFAULT);
    }
    this.updateMarginsDisabled_();
  }

  /** @param {*} newValue The new value of the margins setting. */
  onMarginsSettingChange_(newValue) {
    this.selectedValue =
        /** @type {!MarginsType} */ (newValue).toString();
  }

  /** @param {string} value The new select value. */
  onProcessSelectChange(value) {
    this.setSetting('margins', parseInt(value, 10));
  }

  /** @private */
  updateMarginsDisabled_() {
    this.marginsDisabled_ =
        /** @type {number} */ (this.getSettingValue('pagesPerSheet')) > 1 ||
        this.disabled;
  }
}

customElements.define(
    PrintPreviewMarginsSettingsElement.is, PrintPreviewMarginsSettingsElement);
