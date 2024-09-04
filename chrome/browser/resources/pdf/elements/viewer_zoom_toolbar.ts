// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons_lit.html.js';
import './icons.html.js';
import './viewer_zoom_button.js';

import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {FittingType} from '../constants.js';

import type {ViewerZoomButtonElement} from './viewer_zoom_button.js';
import {getCss} from './viewer_zoom_toolbar.css.js';
import {getHtml} from './viewer_zoom_toolbar.html.js';

const FIT_TO_PAGE_BUTTON_STATE = 0;
const FIT_TO_WIDTH_BUTTON_STATE = 1;

export interface ViewerZoomToolbarElement {
  $: {
    fitButton: ViewerZoomButtonElement,
  };
}

export class ViewerZoomToolbarElement extends CrLitElement {
  static get is() {
    return 'viewer-zoom-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      keyboardNavigationActive_: {type: Boolean},

      pdfCr23Enabled: {type: Boolean},

      visible_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected keyboardNavigationActive_: boolean = false;
  protected pdfCr23Enabled: boolean = false;
  private visible_: boolean = false;

  override firstUpdated() {
    this.addEventListener('focus', this.onFocus_);
    this.addEventListener('keyup', this.onKeyUp_);
    this.addEventListener('pointerdown', this.onPointerDown_);
  }

  isVisible(): boolean {
    return this.visible_;
  }

  protected iconsetName_(): string {
    return this.pdfCr23Enabled ? 'pdf-cr23' : 'pdf';
  }

  private onFocus_(): void {
    if (this.visible_) {
      return;
    }

    // For Print Preview, ensure the parent element knows that keyboard
    // navigation is now active and show the toolbar.
    this.fire('keyboard-navigation-active', true);
    this.show();
  }

  private onKeyUp_(): void {
    this.fire('keyboard-navigation-active', true);
    this.keyboardNavigationActive_ = true;
  }

  private onPointerDown_(): void {
    this.fire('keyboard-navigation-active', false);
    this.keyboardNavigationActive_ = false;
  }

  /** Handle clicks of the fit-button. */
  fitToggle() {
    this.fireFitToChangedEvent_(
        this.$.fitButton.activeIndex === FIT_TO_WIDTH_BUTTON_STATE ?
            FittingType.FIT_TO_WIDTH :
            FittingType.FIT_TO_PAGE);
  }

  /** Handle the keyboard shortcut equivalent of fit-button clicks. */
  fitToggleFromHotKey() {
    this.fitToggle();

    // Toggle the button state since there was no mouse click.
    const button = this.$.fitButton;
    button.activeIndex =
        (button.activeIndex === FIT_TO_WIDTH_BUTTON_STATE ?
             FIT_TO_PAGE_BUTTON_STATE :
             FIT_TO_WIDTH_BUTTON_STATE);
  }

  /**
   * Handle forcing zoom via scripting to a fitting type.
   * @param fittingType Page fitting type to force.
   */
  forceFit(fittingType: FittingType) {
    // Set the button state since there was no mouse click.
    const nextButtonState =
        (fittingType === FittingType.FIT_TO_WIDTH ? FIT_TO_PAGE_BUTTON_STATE :
                                                    FIT_TO_WIDTH_BUTTON_STATE);
    this.$.fitButton.activeIndex = nextButtonState;
  }

  /**
   * Fire a 'fit-to-changed' {CustomEvent} with the given FittingType as detail.
   * @param fittingType to include as payload.
   */
  private fireFitToChangedEvent_(fittingType: FittingType) {
    this.fire('fit-to-changed', fittingType);
  }

  /** Handle clicks of the zoom-in-button. */
  zoomIn() {
    this.fire('zoom-in');
  }

  /** Handle clicks of the zoom-out-button. */
  zoomOut() {
    this.fire('zoom-out');
  }

  show() {
    this.visible_ = true;
  }

  hide() {
    this.visible_ = false;
  }

  /**
   * Offsets the toolbar position so that it doesn't move if scrollbars appear.
   */
  shiftForScrollbars(
      hasScrollbars: {horizontal: boolean, vertical: boolean},
      scrollbarWidth: number) {
    const verticalScrollbarWidth = hasScrollbars.vertical ? scrollbarWidth : 0;
    const horizontalScrollbarWidth =
        hasScrollbars.horizontal ? scrollbarWidth : 0;

    // Shift the zoom toolbar to the left by half a scrollbar width. This
    // gives a compromise: if there is no scrollbar visible then the toolbar
    // will be half a scrollbar width further left than the spec but if there
    // is a scrollbar visible it will be half a scrollbar width further right
    // than the spec. In RTL layout normally, the zoom toolbar is on the left
    // left side, but the scrollbar is still on the right, so this is not
    // necessary.
    if (!isRTL()) {
      this.style.right = -verticalScrollbarWidth + (scrollbarWidth / 2) + 'px';
    }
    // Having a horizontal scrollbar is much rarer so we don't offset the
    // toolbar from the bottom any more than what the spec says. This means
    // that when there is a scrollbar visible, it will be a full scrollbar
    // width closer to the bottom of the screen than usual, but this is ok.
    this.style.bottom = -horizontalScrollbarWidth + 'px';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-zoom-toolbar': ViewerZoomToolbarElement;
  }
}

customElements.define(ViewerZoomToolbarElement.is, ViewerZoomToolbarElement);
