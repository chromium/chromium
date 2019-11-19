// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/shadow.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Size of additional padding in the inner scrollable section of the dropdown.
 */
const DROPDOWN_INNER_PADDING = 12;

/** Size of vertical padding on the outer #dropdown element. */
const DROPDOWN_OUTER_PADDING = 2;

/** Minimum height of toolbar dropdowns (px). */
const MIN_DROPDOWN_HEIGHT = 200;

Polymer({
  is: 'viewer-toolbar-dropdown',

  _template: html`{__html_template__}`,

  properties: {
    /** Icon to display when the dropdown is closed. */
    closedIcon: String,

    /** Whether the dropdown should be centered or right aligned. */
    dropdownCentered: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /** True if the dropdown is currently open. */
    dropdownOpen: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /**
     * String to be displayed at the top of the dropdown and for the tooltip
     * of the button.
      */
    header: String,

    /** Whether to hide the header at the top of the dropdown. */
    hideHeader: {
      type: Boolean,
      value: false,
    },

    /** Lowest vertical point that the dropdown should occupy (px). */
    lowerBound: {
      type: Number,
      observer: 'lowerBoundChanged_',
    },

    /** Unique id to identify this dropdown for metrics purposes. */
    metricsId: String,

    /** Whether the dropdown must be selected before opening. */
    openAfterSelect: {
      type: Boolean,
      value: false,
    },

    /** Icon to display when the dropdown is open. */
    openIcon: String,

    /** Whether the dropdown is marked as selected. */
    selected: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
    },

    /**
     * Toolbar icon currently being displayed.
     * @private
     */
    dropdownIcon_: {
      type: String,
      computed: 'computeIcon_(dropdownOpen, closedIcon, openIcon)',
    },
  },

  /**
   * Current animation being played, or null if there is none.
   * @private {?Object}
   */
  animation_: null,

  /**
   * True if the max-height CSS property for the dropdown scroll container
   * is valid. If false, the height will be updated the next time the
   * dropdown is visible.
   * @private {boolean}
   */
  maxHeightValid_: false,

  /**
   * @return {string} Current icon for the dropdown.
   * @private
   */
  computeIcon_: function(dropdownOpen, closedIcon, openIcon) {
    return dropdownOpen ? openIcon : closedIcon;
  },

  /** @private */
  lowerBoundChanged_: function() {
    this.maxHeightValid_ = false;
    if (this.dropdownOpen) {
      this.updateMaxHeight();
    }
  },

  toggleDropdown: function() {
    if (!this.dropdownOpen && this.openAfterSelect && !this.selected) {
      // The dropdown has `openAfterSelect` set, but is not yet selected.
      return;
    }
    this.dropdownOpen = !this.dropdownOpen;
    if (this.dropdownOpen) {
      this.$.dropdown.style.display = 'block';
      if (!this.maxHeightValid_) {
        this.updateMaxHeight();
      }
      this.fire('dropdown-opened', this.metricsId);
    }

    if (this.dropdownOpen) {
      const listener = (e) => {
        if (e.path.includes(this)) {
          return;
        }
        if (this.dropdownOpen) {
          this.toggleDropdown();
          this.blur();
        }
        // Clean up the handler. The dropdown may already be closed.
        window.removeEventListener('pointerdown', listener);
      };
      window.addEventListener('pointerdown', listener);
    }

    this.playAnimation_(this.dropdownOpen);
  },

  updateMaxHeight: function() {
    const scrollContainer = this.$['scroll-container'];
    let height = this.lowerBound - scrollContainer.getBoundingClientRect().top -
        DROPDOWN_INNER_PADDING;
    height = Math.max(height, MIN_DROPDOWN_HEIGHT);
    scrollContainer.style.maxHeight = height + 'px';
    this.maxHeightValid_ = true;
  },

  /**
   * Start an animation on the dropdown.
   * @param {boolean} isEntry True to play entry animation, false to play
   * exit.
   * @private
   */
  playAnimation_: function(isEntry) {
    this.animation_ = isEntry ? this.animateEntry_() : this.animateExit_();
    this.animation_.onfinish = () => {
      this.animation_ = null;
      if (!this.dropdownOpen) {
        this.$.dropdown.style.display = 'none';
      }
    };
  },

  /**
   * @return {!Object} Animation
   * @private
   */
  animateEntry_: function() {
    let maxHeight =
        this.$.dropdown.getBoundingClientRect().height - DROPDOWN_OUTER_PADDING;

    if (maxHeight < 0) {
      maxHeight = 0;
    }

    this.$.dropdown.animate(
        [{opacity: 0}, {opacity: 1}],
        {
          duration: 150,
          easing: 'cubic-bezier(0, 0, 0.2, 1)',
        });
    return this.$.dropdown.animate(
        [
          {height: '20px', transform: 'translateY(-10px)'},
          {height: maxHeight + 'px', transform: 'translateY(0)'},
        ],
        {
          duration: 250,
          easing: 'cubic-bezier(0, 0, 0.2, 1)',
        });
  },

  /**
   * @return {!Object} Animation
   * @private
   */
  animateExit_: function() {
    return this.$.dropdown.animate(
        [
          {transform: 'translateY(0)', opacity: 1},
          {transform: 'translateY(-5px)', opacity: 0},
        ],
        {
          duration: 100,
          easing: 'cubic-bezier(0.4, 0, 1, 1)',
        });
  }
});
