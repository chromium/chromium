// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-meta/iron-meta.js';
import './icons.js';
import './print_preview_shared_css.js';
import './settings_section.js';

import {Base, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DuplexMode} from '../data/model.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {SelectBehavior, SelectBehaviorInterface} from './select_behavior.js';
import {SettingsBehavior, SettingsBehaviorInterface} from './settings_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {SettingsBehaviorInterface}
 * @implements {SelectBehaviorInterface}
 */
const PrintPreviewDuplexSettingsElementBase =
    mixinBehaviors([SettingsBehavior, SelectBehavior], PolymerElement);

/** @polymer */
export class PrintPreviewDuplexSettingsElement extends
    PrintPreviewDuplexSettingsElementBase {
  static get is() {
    return 'print-preview-duplex-settings';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      dark: Boolean,

      disabled: Boolean,

      /**
       * Mirroring the enum so that it can be used from HTML bindings.
       * @private
       */
      duplexValueEnum_: {
        type: Object,
        value: DuplexMode,
      },
    };
  }

  static get observers() {
    return [
      'onDuplexSettingChange_(settings.duplex.*)',
      'onDuplexTypeChange_(settings.duplexShortEdge.*)',
    ];
  }

  constructor() {
    super();

    /** @private {!IronMetaElement} */
    this.meta_ = /** @type {!IronMetaElement} */ (
        Base.create('iron-meta', {type: 'iconset'}));
  }

  /** @private */
  onDuplexSettingChange_() {
    this.$.duplex.checked = this.getSettingValue('duplex');
  }

  /** @private */
  onDuplexTypeChange_() {
    this.selectedValue = this.getSettingValue('duplexShortEdge') ?
        this.duplexValueEnum_.SHORT_EDGE.toString() :
        this.duplexValueEnum_.LONG_EDGE.toString();
  }

  /** @private */
  onCheckboxChange_() {
    this.setSetting('duplex', this.$.duplex.checked);
  }

  onProcessSelectChange(value) {
    this.setSetting(
        'duplexShortEdge',
        value === this.duplexValueEnum_.SHORT_EDGE.toString());
  }

  /**
   * @return {boolean} Whether to expand the collapse for the dropdown.
   * @private
   */
  getOpenCollapse_() {
    return this.getSetting('duplexShortEdge').available &&
        /** @type {boolean} */ (this.getSettingValue('duplex'));
  }

  /**
   * @param {boolean} managed Whether the setting is managed by policy.
   * @param {boolean} disabled value of this.disabled
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  getDisabled_(managed, disabled) {
    return managed || disabled;
  }

  /**
   * @return {string} An inline svg corresponding to |icon| and the image for
   *     the dropdown arrow.
   * @private
   */
  getBackgroundImages_() {
    const icon =
        this.getSettingValue('duplexShortEdge') ? 'short-edge' : 'long-edge';
    const iconset = /** @type {!IronIconsetSvgElement} */ (
        this.meta_.byKey('print-preview'));
    return getSelectDropdownBackground(iconset, icon, this);
  }
}

customElements.define(
    PrintPreviewDuplexSettingsElement.is, PrintPreviewDuplexSettingsElement);
