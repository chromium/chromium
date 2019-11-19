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

import {Base, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {DuplexMode} from '../data/model.js';
import {getSelectDropdownBackground} from '../print_preview_utils.js';

import {SelectBehavior} from './select_behavior.js';
import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-duplex-settings',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, SelectBehavior],

  properties: {
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
  },

  observers: [
    'onDuplexSettingChange_(settings.duplex.*)',
    'onDuplexTypeChange_(settings.duplexShortEdge.*)',
  ],

  /** @private {!IronMetaElement} */
  meta_: /** @type {!IronMetaElement} */ (
      Base.create('iron-meta', {type: 'iconset'})),

  /** @private */
  onDuplexSettingChange_: function() {
    this.$.duplex.checked = this.getSettingValue('duplex');
  },

  /** @private */
  onDuplexTypeChange_: function() {
    this.selectedValue = this.getSettingValue('duplexShortEdge') ?
        this.duplexValueEnum_.SHORT_EDGE.toString() :
        this.duplexValueEnum_.LONG_EDGE.toString();
  },

  /** @private */
  onCheckboxChange_: function() {
    this.setSetting('duplex', this.$.duplex.checked);
  },

  onProcessSelectChange: function(value) {
    this.setSetting(
        'duplexShortEdge',
        value === this.duplexValueEnum_.SHORT_EDGE.toString());
  },

  /**
   * @return {boolean} Whether to expand the collapse for the dropdown.
   * @private
   */
  getOpenCollapse_: function() {
    return this.getSetting('duplexShortEdge').available &&
        /** @type {boolean} */ (this.getSettingValue('duplex'));
  },

  /**
   * @param {boolean} managed Whether the setting is managed by policy.
   * @param {boolean} disabled value of this.disabled
   * @return {boolean} Whether the controls should be disabled.
   * @private
   */
  getDisabled_: function(managed, disabled) {
    return managed || disabled;
  },

  /**
   * @return {string} An inline svg corresponding to |icon| and the image for
   *     the dropdown arrow.
   * @private
   */
  getBackgroundImages_: function() {
    const icon =
        this.getSettingValue('duplexShortEdge') ? 'short-edge' : 'long-edge';
    const iconset = /** @type {!IronIconsetSvgElement} */ (
        this.meta_.byKey('print-preview'));
    return getSelectDropdownBackground(iconset, icon, this);
  },
});
