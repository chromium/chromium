// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This dialog explains and warns users of the expected outcome
 * when disabling peripheral data access setup.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../settings_shared_css.js';

import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrefsBehavior} from '../prefs_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-peripheral-data-access-protection-dialog',

  behaviors: [
    PrefsBehavior,
  ],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    prefName: {
      type: String,
    },
  },

  /**
   * Closes the warning dialog and transitions to the disabling dialog.
   * @private
   */
  onDisableClicked_() {
    // Send the new state immediately, this will also toggle the underlying
    // setting-toggle-button associated with this pref.
    this.setPrefValue(this.prefName, true);
    this.$$('#warningDialog').close();
  },

  /** @private */
  onCancelButtonClicked_() {
    this.$$('#warningDialog').close();
  },
});
