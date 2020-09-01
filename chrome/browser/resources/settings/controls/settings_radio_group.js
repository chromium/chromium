// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * `settings-radio-group` wraps cr-radio-group and set of radio-buttons that
 * control a supplied preference.
 *
 * Example:
 *      <settings-radio-group pref="{{prefs.settings.foo}}"
 *          label="Foo Options." buttons="{{fooOptionsList}}">
 *      </settings-radio-group>
 */
Polymer({
  is: 'settings-radio-group',

  behaviors: [PrefControlBehavior],

  properties: {
    groupAriaLabel: String,

    /**
     * If true, do not automatically set the preference value. This allows the
     * container to confirm the change first then call either sendPrefChange
     * or resetToPrefValue accordingly.
     */
    noSetPref: {
      type: Boolean,
      value: false,
    },

    selected: String,

    selectableElements: {
      type: String,
      value: ['cr-radio-button', 'controlled-radio-button'].join(', '),
    },
  },

  hostAttributes: {
    role: 'none',
  },

  observers: [
    'resetToPrefValue(pref.*)',
  ],

  /** @override */
  focus() {
    this.$$('cr-radio-group').focus();
  },

  /** Reset the selected value to match the current pref value. */
  resetToPrefValue() {
    const pref = /** @type {!chrome.settingsPrivate.PrefObject} */ (this.pref);
    this.selected = Settings.PrefUtil.prefToString(pref);
  },

  /** Update the pref to the current selected value. */
  sendPrefChange() {
    if (!this.pref) {
      return;
    }
    this.set(
        'pref.value',
        Settings.PrefUtil.stringToPrefValue(this.selected, this.pref));
  },

  /** @private */
  onSelectedChanged_() {
    this.selected = this.$$('cr-radio-group').selected;
    if (!this.noSetPref) {
      this.sendPrefChange();
    }
    this.fire('change');
  },
});
