// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/shadow.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'viewer-zoom-button',

  _template: html`{__html_template__}`,

  properties: {
    /** Index of the icon currently being displayed. */
    activeIndex: {
      type: Number,
      value: 0,
    },

    delay: {
      type: Number,
      observer: 'delayChanged_',
    },

    /**
     * Icons to be displayed on the FAB. Multiple icons should be separated with
     * spaces, and will be cycled through every time the FAB is clicked.
     */
    icons: String,

    /**
     * Used to show the appropriate drop shadow when buttons are focused with
     * the keyboard.
     */
    keyboardNavigationActive: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {?Array<string>} */
    tooltips: Array,

    /** @private */
    closed_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /**
     * Array version of the list of icons. Polymer does not allow array
     * properties to be set from HTML, so we must use a string property and
     * perform the conversion manually.
     * @private {!Array<string>}
     */
    icons_: {
      type: Array,
      value: [''],
      computed: 'computeIconsArray_(icons)',
    },

    /**
     * Icon currently being displayed on the FAB.
     * @private
     */
    visibleIcon_: {
      type: String,
      computed: 'computeVisibleIcon_(icons_, activeIndex)',
    },

    /** @private */
    visibleTooltip_: {
      type: String,
      computed: 'computeVisibleTooltip_(tooltips, activeIndex)',
    }
  },

  /**
   * @param {string} icons Icon names in a string, delimited by spaces
   * @return {!Array<string>} Array of icon name strings
   * @private
   */
  computeIconsArray_: function(icons) {
    return icons.split(' ');
  },

  /**
   * @param {!Array<string>} icons Array of icon name strings.
   * @param {number} activeIndex Index of the currently active icon.
   * @return {string} Icon name for the currently visible icon.
   * @private
   */
  computeVisibleIcon_: function(icons, activeIndex) {
    return icons[activeIndex];
  },

  /**
   * @param {?Array<string>} tooltips Array of tooltip strings.
   * @param {number} activeIndex Index of the currently active icon.
   * @return {string} Tooltip for the currently visible icon.
   * @private
   */
  computeVisibleTooltip_: function(tooltips, activeIndex) {
    return tooltips === undefined ? '' : tooltips[activeIndex];
  },

  /** @private */
  delayChanged_: function() {
    this.$.wrapper.style.transitionDelay = this.delay + 'ms';
  },

  show: function() {
    this.closed_ = false;
  },

  hide: function() {
    this.closed_ = true;
  },

  /** @private */
  fireClick_: function() {
    // We cannot attach an on-click to the entire viewer-zoom-button, as this
    // will include clicks on the margins. Instead, proxy clicks on the FAB
    // through.
    this.fire('fabclick');

    this.activeIndex = (this.activeIndex + 1) % this.icons_.length;
  }
});
