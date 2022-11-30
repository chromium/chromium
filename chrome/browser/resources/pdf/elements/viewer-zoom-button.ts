// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/paper-styles/shadow.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './viewer-zoom-button.html.js';

export class ViewerZoomButtonElement extends PolymerElement {
  static get is() {
    return 'viewer-zoom-button';
  }

  static get template() {
    return getTemplate();
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
       */
      icons_: {
        type: Array,
        value: [''],
        computed: 'computeIconsArray_(icons)',
      },

      tooltips_: {
        type: Array,
        computed: 'computeTooltipsArray_(tooltips)',
      },

      /**
       * Icon currently being displayed on the FAB.
       */
      visibleIcon_: {
        type: String,
        computed: 'computeVisibleIcon_(icons_, activeIndex)',
      },

      visibleTooltip_: {
        type: String,
        computed: 'computeVisibleTooltip_(tooltips_, activeIndex)',
      },
    };
  }

  activeIndex: number;
  disabled: boolean;
  icons: string;
  keyboardNavigationActive: boolean;
  tooltips: string;
  private icons_: string[];
  private tooltips_: string[];
  private visibleIcon_: string;
  private visibleTooltip_: string;

  private computeIconsArray_(): string[] {
    return this.icons.split(' ');
  }

  private computeTooltipsArray_(): string[] {
    return this.tooltips.split(',');
  }

  /**
   * @return Icon name for the currently visible icon.
   */
  private computeVisibleIcon_(): string {
    return this.icons_[this.activeIndex];
  }

  /**
   * @return Tooltip for the currently visible icon.
   */
  private computeVisibleTooltip_(): string {
    return this.tooltips_ === undefined ? '' : this.tooltips_[this.activeIndex];
  }

  private fireClick_() {
    // We cannot attach an on-click to the entire viewer-zoom-button, as this
    // will include clicks on the margins. Instead, proxy clicks on the FAB
    // through.
    this.dispatchEvent(
        new CustomEvent('fabclick', {bubbles: true, composed: true}));

    this.activeIndex = (this.activeIndex + 1) % this.icons_.length;
  }
}

customElements.define(ViewerZoomButtonElement.is, ViewerZoomButtonElement);
