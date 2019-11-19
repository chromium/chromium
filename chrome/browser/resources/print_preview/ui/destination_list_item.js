// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './print_preview_vars_css.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {removeHighlights} from 'chrome://resources/js/search_highlight_utils.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination, DestinationOrigin} from '../data/destination.js';

import {HighlightResults, updateHighlights} from './highlight_utils.js';


// <if expr="chromeos">
/** @enum {number} */
const DestinationConfigStatus = {
  IDLE: 0,
  IN_PROGRESS: 1,
  FAILED: 2,
};
// </if>

Polymer({
  is: 'print-preview-destination-list-item',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {!Destination} */
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
    /** @private {!DestinationConfigStatus} */
    configurationStatus_: {
      type: Number,
      value: DestinationConfigStatus.IDLE,
    },

    /**
     * Mirroring the enum so that it can be used from HTML bindings.
     * @private
     */
    statusEnum_: {
      type: Object,
      value: DestinationConfigStatus,
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
        this.destination.origin == DestinationOrigin.CROS &&
        !this.destination.capabilities);
    this.configurationStatus_ = DestinationConfigStatus.IN_PROGRESS;
  },

  /**
   * Called when the printer configuration request completes.
   * @param {boolean} success Whether configuration was successful.
   */
  onConfigureComplete: function(success) {
    this.configurationStatus_ =
        success ? DestinationConfigStatus.IDLE : DestinationConfigStatus.FAILED;
  },

  /**
   * @param {!DestinationConfigStatus} status
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
    removeHighlights(this.highlights_);
    this.highlights_ = this.updateHighlighting_().highlights;
  },

  /** @private */
  updateSearchHint_: function() {
    const matches = !this.searchQuery ?
        [] :
        this.destination.extraPropertiesToMatch.filter(
            p => p.match(this.searchQuery));
    this.searchHint_ = matches.length === 0 ?
        (this.destination.extraPropertiesToMatch.find(p => !!p) || '') :
        matches.join(' ');
  },

  /**
   * @return {!HighlightResults} The highlight wrappers and
   *     search bubbles that were created.
   * @private
   */
  updateHighlighting_: function() {
    return updateHighlights(this, this.searchQuery);
  },

  /**
   * @return {string} A tooltip for the extension printer icon.
   * @private
   */
  getExtensionPrinterTooltip_: function() {
    if (!this.destination.isExtension) {
      return '';
    }
    return loadTimeData.getStringF(
        'extensionDestinationIconTooltip', this.destination.extensionName);
  },
});
