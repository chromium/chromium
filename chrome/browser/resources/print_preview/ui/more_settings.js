// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Metrics, MetricsContext} from '../metrics.js';

Polymer({
  is: 'print-preview-more-settings',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    settingsExpandedByUser: {
      type: Boolean,
      notify: true,
    },

    disabled: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },

  /** @private {!MetricsContext} */
  metrics_: MetricsContext.printSettingsUi(),

  /**
   * Toggles the expand button within the element being listened to.
   * @param {!Event} e
   * @private
   */
  toggleExpandButton_: function(e) {
    // The expand button handles toggling itself.
    const expandButtonTag = 'CR-EXPAND-BUTTON';
    if (e.target.tagName == expandButtonTag) {
      return;
    }

    if (!e.currentTarget.hasAttribute('actionable')) {
      return;
    }

    /** @type {!CrExpandButtonElement} */
    const expandButton = e.currentTarget.querySelector(expandButtonTag);
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
    this.metrics_.record(
        this.settingsExpandedByUser ?
            Metrics.PrintSettingsUiBucket.MORE_SETTINGS_CLICKED :
            Metrics.PrintSettingsUiBucket.LESS_SETTINGS_CLICKED);
  },
});
