// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('print_preview_new');

// <if expr="chromeos">
/** @enum {number} */
print_preview_new.DestinationConfigStatus = {
  IDLE: 0,
  IN_PROGRESS: 1,
  FAILED: 2,
};
// </if>

Polymer({
  is: 'print-preview-destination-list-item',

  properties: {
    /** @type {!print_preview.Destination} */
    destination: Object,

    /** @type {?RegExp} */
    searchQuery: Object,

    /** @private */
    stale_: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @private {string} */
    searchHint_: String,

    // <if expr="chromeos">
    /** @private {!print_preview_new.DestinationConfigStatus} */
    configurationStatus_: {
      type: Number,
      value: print_preview_new.DestinationConfigStatus.IDLE,
    },

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    statusEnum_: {
      type: Object,
      value: print_preview_new.DestinationConfigStatus,
    },
    // </if>
  },

  observers: [
    'onDestinationPropertiesChange_(' +
        'destination.displayName, destination.isOfflineOrInvalid, ' +
        'destination.isExtension)',
    'updateHighlightsAndHint_(destination, searchQuery)',
  ],

  /** @private {!Array<!Node>} */
  highlights_: [],

  /** @private */
  onDestinationPropertiesChange_: function() {
    this.title = this.destination.displayName;
    this.stale_ = this.destination.isOfflineOrInvalid;
    if (this.destination.isExtension) {
      const icon = this.$$('.extension-icon');
      icon.style.backgroundImage = '-webkit-image-set(' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/24/1) 1x,' +
          'url(chrome://extension-icon/' + this.destination.extensionId +
          '/48/1) 2x)';
    }
  },

  // <if expr="chromeos">
  /**
   * Called if the printer configuration request is accepted. Show the waiting
   * message to the user as the configuration might take longer than expected.
   */
  onConfigureRequestAccepted: function() {
    // It must be a Chrome OS CUPS printer which hasn't been set up before.
    assert(
        this.destination.origin == print_preview.DestinationOrigin.CROS &&
        !this.destination.capabilities);
    this.configurationStatus_ =
        print_preview_new.DestinationConfigStatus.IN_PROGRESS;
  },

  /**
   * Called when the printer configuration request completes.
   * @param {boolean} success Whether configuration was successful.
   */
  onConfigureComplete: function(success) {
    this.configurationStatus_ = success ?
        print_preview_new.DestinationConfigStatus.IDLE :
        print_preview_new.DestinationConfigStatus.FAILED;
  },

  /**
   * @param {!print_preview_new.DestinationConfigStatus} status
   * @return {boolean} Whether the current configuration status is |status|.
   * @private
   */
  checkConfigurationStatus_: function(status) {
    return this.configurationStatus_ == status;
  },
  // </if>

  /** @private */
  updateHighlightsAndHint_: function() {
    this.updateSearchHint_();
    cr.search_highlight_utils.removeHighlights(this.highlights_);
    this.highlights_ = this.updateHighlighting_().highlights;
  },

  /** @private */
  updateSearchHint_: function() {
    this.searchHint_ = !this.searchQuery ?
        '' :
        this.destination.extraPropertiesToMatch
            .filter(p => p.match(this.searchQuery))
            .join(' ');
  },

  /**
   * @return {!print_preview.HighlightResults} The highlight wrappers and
   *     search bubbles that were created.
   * @private
   */
  updateHighlighting_: function() {
    return print_preview.updateHighlights(this, this.searchQuery);
  },
});
