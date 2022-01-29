// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/polymer/v3_0/paper-styles/shadow.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export class ViewerZoomButtonElement extends PolymerElement {
  static get is() {
    return 'viewer-zoom-button';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Index of the icon currently being displayed. */
      activeIndex: {
        type: Number,
        value: 0,
      },

      disabled: {
        type: Boolean,
        value: false,
      },

      /**
       * Icons to be displayed on the FAB. Multiple icons should be separated
       * with spaces, and will be cycled through every time the FAB is clicked.
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

      tooltips: String,

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

      /** @private {!Array<string>} */
      tooltips_: {
        type: Array,
        computed: 'computeTooltipsArray_(tooltips)',
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
        computed: 'computeVisibleTooltip_(tooltips_, activeIndex)',
      },
    };
  }

  /**
   * @return {!Array<string>} Array of icon name strings
   * @private
   */
  computeIconsArray_() {
    return this.icons.split(' ');
  }

  /**
   * @return {!Array<string>} Array of tooltip strings
   * @private
   */
  computeTooltipsArray_() {
    return this.tooltips.split(',');
  }

  /**
   * @return {string} Icon name for the currently visible icon.
   * @private
   */
  computeVisibleIcon_() {
    return this.icons_[this.activeIndex];
  }

  /**
   * @return {string} Tooltip for the currently visible icon.
   * @private
   */
  computeVisibleTooltip_() {
    return this.tooltips_ === undefined ? '' : this.tooltips_[this.activeIndex];
  }

  /** @private */
  fireClick_() {
    // We cannot attach an on-click to the entire viewer-zoom-button, as this
    // will include clicks on the margins. Instead, proxy clicks on the FAB
    // through.
    this.dispatchEvent(
        new CustomEvent('fabclick', {bubbles: true, composed: true}));

    this.activeIndex = (this.activeIndex + 1) % this.icons_.length;
  }
}

customElements.define(ViewerZoomButtonElement.is, ViewerZoomButtonElement);
