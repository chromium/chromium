// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-advanced-dialog',

  behaviors: [SettingsBehavior, I18nBehavior],

  properties: {
    /** @type {!print_preview.Destination} */
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

  /** @private {!print_preview.PrintSettingsUiMetricsContext} */
  metrics_: new print_preview.PrintSettingsUiMetricsContext(),

  /** @override */
  attached: function() {
    this.metrics_.record(print_preview.Metrics.PrintSettingsUiBucket
                             .ADVANCED_SETTINGS_DIALOG_SHOWN);
    this.$.dialog.showModal();
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_: function(e) {
    e.stopPropagation();
    const searchInput = this.$.searchBox.getSearchInput();
    if (e.key == 'Escape' &&
        (e.composedPath()[0] !== searchInput || !searchInput.value.trim())) {
      this.$.dialog.cancel();
      e.preventDefault();
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
    if (!this.shadowRoot)
      return true;

    cr.search_highlight_utils.removeHighlights(this.highlights_);
    for (let bubble of this.bubbles_)
      bubble.remove();
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
      this.highlights_.push.apply(this.highlights_, result.highlights);
      this.bubbles_.push.apply(this.bubbles_, result.bubbles);
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
    if (this.searchQuery_)
      this.$.searchBox.setValue('');
    if (this.$.dialog.getNative().returnValue == 'success') {
      this.metrics_.record(print_preview.Metrics.PrintSettingsUiBucket
                               .ADVANCED_SETTINGS_DIALOG_CANCELED);
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
