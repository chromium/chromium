// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import './advanced_settings_item.js';
import './print_preview_search_box.js';
import './print_preview_shared_css.js';
import './print_preview_vars_css.js';
import '../strings.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';
import {Metrics, MetricsContext} from '../metrics.js';

import {SettingsBehavior} from './settings_behavior.js';

Polymer({
  is: 'print-preview-advanced-settings-dialog',

  _template: html`{__html_template__}`,

  behaviors: [SettingsBehavior, I18nBehavior],

  properties: {
    /** @type {!Destination} */
    destination: Object,

    /** @private {?RegExp} */
    searchQuery_: {
      type: Object,
      value: null,
    },

    /** @private {boolean} */
    hasMatching_: {
      type: Boolean,
      notify: true,
      computed: 'computeHasMatching_(searchQuery_)',
    },
  },

  listeners: {
    'keydown': 'onKeydown_',
  },

  /** @private {!Array<Node>} */
  highlights_: [],

  /** @private {!Array<Node>} */
  bubbles_: [],

  /** @private {!MetricsContext} */
  metrics_: MetricsContext.printSettingsUi(),

  /** @override */
  attached: function() {
    this.metrics_.record(
        Metrics.PrintSettingsUiBucket.ADVANCED_SETTINGS_DIALOG_SHOWN);
    this.$.dialog.showModal();
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_: function(e) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    const eventInSearchBox = e.composedPath().includes(searchInput);
    if (e.key == 'Escape' && (!eventInSearchBox || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
      return;
    }

    if (e.key == 'Enter' && !eventInSearchBox) {
      const activeElementTag = e.composedPath()[0].tagName;
      if (['CR-BUTTON', 'SELECT'].includes(activeElementTag)) {
        return;
      }
      this.onApplyButtonClick_();
      e.preventDefault();
      return;
    }
  },

  /**
   * @return {boolean} Whether there is more than one vendor item to display.
   * @private
   */
  hasMultipleItems_: function() {
    return this.destination.capabilities.printer.vendor_capability.length > 1;
  },

  /**
   * @return {boolean} Whether there is a setting matching the query.
   * @private
   */
  computeHasMatching_: function() {
    if (!this.shadowRoot) {
      return true;
    }

    removeHighlights(this.highlights_);
    for (const bubble of this.bubbles_) {
      bubble.remove();
    }
    this.highlights_ = [];
    this.bubbles_ = [];

    const listItems = this.shadowRoot.querySelectorAll(
        'print-preview-advanced-settings-item');
    let hasMatch = false;
    listItems.forEach(item => {
      const matches = item.hasMatch(this.searchQuery_);
      item.hidden = !matches;
      hasMatch = hasMatch || matches;
      const result = item.updateHighlighting(this.searchQuery_);
      this.highlights_.push(...result.highlights);
      this.bubbles_.push(...result.bubbles);
    });
    return hasMatch;
  },

  /**
   * @return {boolean} Whether the no matching settings hint should be shown.
   * @private
   */
  shouldShowHint_: function() {
    return !!this.searchQuery_ && !this.hasMatching_;
  },

  /** @private */
  onCloseOrCancel_: function() {
    if (this.searchQuery_) {
      this.$.searchBox.setValue('');
    }
    if (this.$.dialog.getNative().returnValue == 'success') {
      this.metrics_.record(
          Metrics.PrintSettingsUiBucket.ADVANCED_SETTINGS_DIALOG_CANCELED);
    }
  },

  /** @private */
  onCancelButtonClick_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onApplyButtonClick_: function() {
    const settingsValues = {};
    this.shadowRoot.querySelectorAll('print-preview-advanced-settings-item')
        .forEach(item => {
          settingsValues[item.capability.id] = item.getCurrentValue();
        });
    this.setSetting('vendorItems', settingsValues);
    this.$.dialog.close();
  },

  close: function() {
    this.$.dialog.close();
  },
});
