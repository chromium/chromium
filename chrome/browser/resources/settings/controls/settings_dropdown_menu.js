// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * 'settings-dropdown-menu' is a control for displaying options
 * in the settings.
 *
 * Example:
 *
 *   <settings-dropdown-menu pref="{{prefs.foo}}">
 *   </settings-dropdown-menu>
 */
import '//resources/cr_elements/md_select_css.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '../settings_shared_css.js';
import '../settings_vars_css.js';

import {CrPolicyPrefBehavior} from '//resources/cr_elements/policy/cr_policy_pref_behavior.m.js';
import {assert} from '//resources/js/assert.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {prefToString, stringToPrefValue} from '../prefs/pref_util.js';

import {PrefControlBehavior} from './pref_control_behavior.js';

/**
 * The |name| is shown in the gui.  The |value| us use to set or compare with
 * the preference value.
 * @typedef {{
 *   name: string,
 *   value: (number|string)
 * }}
 */
let DropdownMenuOption;

/**
 * @typedef {!Array<!DropdownMenuOption>}
 */
export let DropdownMenuOptionList;

Polymer({
  is: 'settings-dropdown-menu',

  _template: html`{__html_template__}`,

  behaviors: [CrPolicyPrefBehavior, PrefControlBehavior],

  properties: {
    /**
     * List of options for the drop-down menu.
     * @type {!DropdownMenuOptionList}
     */
    menuOptions: Array,

    /** Whether the dropdown menu should be disabled. */
    disabled: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /**
       If this is a dictionary pref, this is the key for the item
        we are interested in.
     */
    prefKey: {
      type: String,
      value: null,
    },

    /**
     * The value of the "custom" item.
     * @private
     */
    notFoundValue_: {
      type: String,
      value: 'SETTINGS_DROPDOWN_NOT_FOUND_ITEM',
      readOnly: true,
    },

    /** Label for a11y purposes */
    label: String,
  },

  observers: [
    'updateSelected_(menuOptions, pref.value.*, prefKey)',
  ],

  /** @override */
  focus() {
    this.$.dropdownMenu.focus();
  },

  /**
   * Pass the selection change to the pref value.
   * @private
   */
  onChange_() {
    const selected = this.$.dropdownMenu.value;

    if (selected === this.notFoundValue_) {
      return;
    }

    if (this.prefKey) {
      assert(this.pref);
      this.set(`pref.value.${this.prefKey}`, selected);
    } else {
      const prefValue = stringToPrefValue(selected, assert(this.pref));
      if (prefValue !== undefined) {
        this.set('pref.value', prefValue);
      }
    }

    // settings-control-change only fires when the selection is changed to
    // a valid property.
    this.fire('settings-control-change');
  },

  /**
   * Updates the selected item when the pref or menuOptions change.
   * @private
   */
  updateSelected_() {
    if (this.menuOptions === undefined || this.pref === undefined ||
        this.prefKey === undefined) {
      return;
    }

    if (!this.menuOptions.length) {
      return;
    }

    const prefValue = this.prefStringValue_();
    const option = this.menuOptions.find(function(menuItem) {
      return menuItem.value.toString() === prefValue;
    });

    // Wait for the dom-repeat to populate the <select> before setting
    // <select>#value so the correct option gets selected.
    this.async(() => {
      this.$.dropdownMenu.value =
          option === undefined ? this.notFoundValue_ : prefValue;
    });
  },

  /**
   * Gets the current value of the preference as a string.
   * @return {string}
   * @private
   */
  prefStringValue_() {
    if (this.prefKey) {
      // Dictionary pref, values are always strings.
      return this.pref.value[this.prefKey];
    } else {
      return prefToString(assert(this.pref));
    }
  },

  /**
   * @param {?DropdownMenuOptionList} menuOptions
   * @param {string} prefValue
   * @return {boolean}
   * @private
   */
  showNotFoundValue_(menuOptions, prefValue) {
    if (menuOptions === undefined || prefValue === undefined) {
      return false;
    }

    // Don't show "Custom" before the options load.
    if (menuOptions === null || menuOptions.length === 0) {
      return false;
    }

    const option = menuOptions.find((menuItem) => {
      return menuItem.value.toString() === this.prefStringValue_();
    });
    return !option;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldDisableMenu_() {
    return this.disabled || this.isPrefEnforced() ||
        this.menuOptions === undefined || this.menuOptions.length === 0;
  },
});
