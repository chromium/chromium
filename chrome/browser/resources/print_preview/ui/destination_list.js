// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import './destination_list_item.js';
import './print_preview_vars_css.js';
import '../strings.m.js';
import './throbber_css.js';

import {ListPropertyUpdateBehavior} from 'chrome://resources/js/list_property_update_behavior.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Destination} from '../data/destination.js';

Polymer({
  is: 'print-preview-destination-list',

  _template: html`{__html_template__}`,

  behaviors: [ListPropertyUpdateBehavior],

  properties: {
    /** @type {Array<!Destination>} */
    destinations: Array,

    /** @type {?RegExp} */
    searchQuery: Object,

    /** @type {boolean} */
    loadingDestinations: {
      type: Boolean,
      value: false,
    },

    listName: String,

    /** @private {!Array<!Destination>} */
    matchingDestinations_: {
      type: Array,
      value: () => [],
    },

    /** @private {boolean} */
    hasDestinations_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    throbberHidden_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'updateMatchingDestinations_(destinations.*, searchQuery)',
    'matchingDestinationsChanged_(' +
        'matchingDestinations_.*, loadingDestinations)',
    'updateThrobberHidden_(matchingDestinations_.*, loadingDestinations)',
  ],

  // This is a workaround to ensure that the iron-list correctly updates the
  // displayed destination information when the elements in the
  // |matchingDestinations_| array change, instead of using stale information
  // (a known iron-list issue). The event needs to be fired while the list is
  // visible, so firing it immediately when the change occurs does not always
  // work.
  forceIronResize: function() {
    this.$.list.fire('iron-resize');
  },

  /** @private */
  updateMatchingDestinations_: function() {
    if (this.destinations === undefined) {
      return;
    }

    this.updateList(
        'matchingDestinations_', destination => destination.key,
        this.searchQuery ?
            this.destinations.filter(
                d => d.matches(/** @type {!RegExp} */ (this.searchQuery))) :
            this.destinations.slice());
  },

  /** @private */
  matchingDestinationsChanged_: function() {
    const count = this.matchingDestinations_.length;
    this.hasDestinations_ = count > 0 || this.loadingDestinations;
  },

  /**
   * @param {!KeyboardEvent} e Event containing the destination and key.
   * @private
   */
  onKeydown_: function(e) {
    if (e.key === 'Enter') {
      this.onDestinationSelected_(e);
      e.stopPropagation();
    }
  },

  /**
   * @param {!Event} e Event containing the destination that was selected.
   * @private
   */
  onDestinationSelected_: function(e) {
    if (e.composedPath()[0].tagName === 'A') {
      return;
    }

    this.fire('destination-selected', e.target);
  },

  /** @private */
  updateThrobberHidden_: function() {
    if (!this.loadingDestinations) {
      this.throbberHidden_ = true;
    } else if (!this.matchingDestinations_) {
      this.throbberHidden_ = false;
    } else {
      const maxDisplayedItems = this.offsetHeight / 32;
      this.throbberHidden_ =
          maxDisplayedItems <= this.matchingDestinations_.length;
    }
    afterNextRender(this, () => {
      this.forceIronResize();
    });
  }
});
