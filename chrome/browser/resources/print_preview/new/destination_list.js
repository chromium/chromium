// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';

Polymer({
  is: 'print-preview-destination-list',

  behaviors: [I18nBehavior, ListPropertyUpdateBehavior],

  properties: {
    /** @type {Array<!print_preview.Destination>} */
    destinations: Array,

    /** @type {?RegExp} */
    searchQuery: Object,

    /** @type {boolean} */
    hasManageLink: {
      type: Boolean,
      value: false,
    },

    /** @type {boolean} */
    loadingDestinations: {
      type: Boolean,
      value: false,
      observer: 'forceIronResize',
    },

    listName: String,

    /** @private {!Array<!print_preview.Destination>} */
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
    showDestinationsTotal_: {
      type: Boolean,
      value: false,
    },
  },

  observers: [
    'updateMatchingDestinations_(destinations.*, searchQuery)',
    'matchingDestinationsChanged_(matchingDestinations_.*)',
  ],

  /** @private {?ResizeObserver} */
  resizeObserver_: null,

  attached: function() {
    this.resizeObserver_ = new ResizeObserver(entries => {
      if (entries === null)
        return;

      const entry = assert(entries[0]);
      // Don't set maxHeight below the minimum height.
      const fullHeight = Math.max(entry.contentRect.height, 64);
      this.$.list.style.maxHeight = `${fullHeight}px`;
      this.forceIronResize();
    });
    this.resizeObserver_.observe(this.$.listContainer);
  },

  detached: function() {
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
  },

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
    if (this.destinations === undefined)
      return;

    this.updateList(
        'matchingDestinations_',
        destination => destination.origin + '/' + destination.id + '/' +
            destination.connectionStatusText,
        this.searchQuery ?
            this.destinations.filter(
                d => d.matches(/** @type {!RegExp} */ (this.searchQuery))) :
            this.destinations.slice());
    this.forceIronResize();
  },

  /** @private */
  matchingDestinationsChanged_: function() {
    const count = this.matchingDestinations_.length;
    this.hasDestinations_ = count > 0;
    this.showDestinationsTotal_ = count > 4;
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
    if (e.composedPath()[0].tagName === 'A')
      return;

    this.fire('destination-selected', e.target);
  },
});
})();
