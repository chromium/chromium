// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons.html.js';
import './icons.html.js';
import './viewer_zoom_button.js';

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

      visible_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  protected accessor keyboardNavigationActive_: boolean = false;
  private accessor visible_: boolean = false;

  override firstUpdated() {
    this.addEventListener('focus', this.onFocus_);
    this.addEventListener('keyup', this.onKeyUp_);
    this.addEventListener('pointerdown', this.onPointerDown_);
  }

  isVisible(): boolean {
    return this.visible_;
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

  protected onFitButtonFabclick_() {
    this.fitToggle();
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
  protected onZoomInFabclick_() {
    this.fire('zoom-in');
  }

  /** Handle clicks of the zoom-out-button. */
  protected onZoomOutFabclick_() {
    this.fire('zoom-out');
  }

  show() {
    this.visible_ = true;
  }

  hide() {
    this.visible_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-zoom-toolbar': ViewerZoomToolbarElement;
  }
}

customElements.define(ViewerZoomToolbarElement.is, ViewerZoomToolbarElement);
