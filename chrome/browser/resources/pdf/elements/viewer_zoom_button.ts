// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './viewer_zoom_button.css.js';
import {getHtml} from './viewer_zoom_button.html.js';

export class ViewerZoomButtonElement extends CrLitElement {
  static get is() {
    return 'viewer-zoom-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /** Index of the icon currently being displayed. */
      activeIndex: {type: Number},

      disabled: {type: Boolean},

      /**
       * Icons to be displayed on the FAB. Multiple icons should be separated
       * with spaces, and will be cycled through every time the FAB is clicked.
       */
      icons: {type: String},

      /**
       * Used to show the appropriate drop shadow when buttons are focused with
       * the keyboard.
       */
      keyboardNavigationActive: {
        type: Boolean,
        reflect: true,
      },

      tooltips: {type: String},

      /**
       * Array version of the list of icons. The public property is a string for
       * convenience so that either a single icon or multiple icons can be
       * easily specified without a data binding in the parent's HTML template.
       */
      icons_: {type: Array},

      tooltips_: {type: Array},
    };
  }

  activeIndex: number = 0;
  disabled: boolean = false;
  icons: string = '';
  keyboardNavigationActive: boolean = false;
  tooltips: string = '';
  private icons_: string[] = [''];
  private tooltips_: string[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('icons')) {
      this.icons_ = this.icons.split(' ');
    }
    if (changedProperties.has('tooltips')) {
      this.tooltips_ = this.tooltips.split(',');
    }
  }

  /**
   * @return Icon name for the currently visible icon.
   */
  protected computeVisibleIcon_(): string {
    return this.icons_[this.activeIndex]!;
  }

  /**
   * @return Tooltip for the currently visible icon.
   */
  protected computeVisibleTooltip_(): string {
    return this.tooltips_ === undefined ? '' : this.tooltips_[this.activeIndex]!
        ;
  }

  protected fireClick_() {
    // We cannot attach an on-click to the entire viewer-zoom-button, as this
    // will include clicks on the margins. Instead, proxy clicks on the FAB
    // through.
    this.fire('fabclick');
    this.activeIndex = (this.activeIndex + 1) % this.icons_.length;
  }
}

customElements.define(ViewerZoomButtonElement.is, ViewerZoomButtonElement);
