// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.m.js';

import {CrRadioButtonBehavior} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-collapse-radio-button',

  _template: html`{__html_template__}`,

  behaviors: [
    CrRadioButtonBehavior,
  ],

  properties: {
    expanded: {
      type: Boolean,
      notify: true,
      value: false,
    },

    noAutomaticCollapse: {
      type: Boolean,
      value: false,
    },

    noCollapse: Boolean,

    label: String,

    icon: {
      type: String,
      value: null,
    },

    /*
     * The Preference associated with the radio group.
     * @type {!chrome.settingsPrivate.PrefObject|undefined}
     */
    pref: Object,

    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    subLabel: {
      type: String,
      value: '',  // Allows the $hidden= binding to run without being set.
    },
  },

  observers: [
    'onCheckedChanged_(checked)',
    'onPrefChanged_(pref.*)',
  ],

  /**
   * Tracks if this button was clicked but wasn't expanded.
   * @private
   */
  pendingUpdateCollapsed_: false,

  /**
   * Updates the collapsed status of this radio button to reflect
   * the user selection actions.
   * @public
   */
  updateCollapsed() {
    if (this.pendingUpdateCollapsed_) {
      this.pendingUpdateCollapsed_ = false;
      this.expanded = this.checked;
    }
  },

  /** @private */
  onCheckedChanged_() {
    this.pendingUpdateCollapsed_ = true;
    if (!this.noAutomaticCollapse) {
      this.updateCollapsed();
    }
  },

  /** @private */
  onPrefChanged_() {
    // If the preference has been set, and is managed, this control should be
    // disabled. Unless the value associated with this control is present in
    // |pref.userSelectableValues|. This will override the disabled set on the
    // element externally.
    this.disabled = !!this.pref &&
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED &&
        !(!!this.pref.userSelectableValues &&
          this.pref.userSelectableValues.includes(this.name));
  },

  /** @private */
  onExpandClicked_() {
    this.fire('expand-clicked');
  },
});
