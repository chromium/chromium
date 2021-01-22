// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import './icons.js';
import './viewer-zoom-button.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {isRTL} from 'chrome://resources/js/util.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FittingType} from '../constants.js';

const FIT_TO_PAGE_BUTTON_STATE = 0;
const FIT_TO_WIDTH_BUTTON_STATE = 1;

const TWO_UP_VIEW_DISABLED_STATE = 0;
const TWO_UP_VIEW_ENABLED_STATE = 1;

Polymer({
  is: 'viewer-zoom-toolbar',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    keyboardNavigationActive_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'focus': 'onFocus_',
    'keyup': 'onKeyUp_',
    'pointerdown': 'onPointerDown_',
  },

  /** @private {boolean} */
  visible_: true,

  /** @return {boolean} */
  isVisible() {
    return this.visible_;
  },

  /** @private */
  onFocus_() {
    if (this.visible_) {
      return;
    }

    // For Print Preview, ensure the parent element knows that keyboard
    // navigation is now active and show the toolbar.
    this.fire('keyboard-navigation-active', true);
    this.show();
  },

  /** @private */
  onKeyUp_() {
    this.fire('keyboard-navigation-active', true);
    this.keyboardNavigationActive_ = true;
  },

  /** @private */
  onPointerDown_() {
    this.fire('keyboard-navigation-active', false);
    this.keyboardNavigationActive_ = false;
  },

  /** Handle clicks of the fit-button. */
  fitToggle() {
    this.fireFitToChangedEvent_(
        this.$['fit-button'].activeIndex === FIT_TO_WIDTH_BUTTON_STATE ?
            FittingType.FIT_TO_WIDTH :
            FittingType.FIT_TO_PAGE);
  },

  /** Handle the keyboard shortcut equivalent of fit-button clicks. */
  fitToggleFromHotKey() {
    this.fitToggle();

    // Toggle the button state since there was no mouse click.
    const button = this.$['fit-button'];
    button.activeIndex =
        (button.activeIndex === FIT_TO_WIDTH_BUTTON_STATE ?
             FIT_TO_PAGE_BUTTON_STATE :
             FIT_TO_WIDTH_BUTTON_STATE);
  },

  /**
   * Handle forcing zoom via scripting to a fitting type.
   * @param {!FittingType} fittingType Page fitting type to force.
   */
  forceFit(fittingType) {
    // Set the button state since there was no mouse click.
    const nextButtonState =
        (fittingType === FittingType.FIT_TO_WIDTH ? FIT_TO_PAGE_BUTTON_STATE :
                                                    FIT_TO_WIDTH_BUTTON_STATE);
    this.$['fit-button'].activeIndex = nextButtonState;
  },

  /**
   * Fire a 'fit-to-changed' {CustomEvent} with the given FittingType as detail.
   * @param {!FittingType} fittingType to include as payload.
   * @private
   */
  fireFitToChangedEvent_(fittingType) {
    this.fire('fit-to-changed', fittingType);
  },

  /** Handle clicks of the zoom-in-button. */
  zoomIn() {
    this.fire('zoom-in');
  },

  /** Handle clicks of the zoom-out-button. */
  zoomOut() {
    this.fire('zoom-out');
  },

  show() {
    if (!this.visible_) {
      this.visible_ = true;
      this.$['fit-button'].show();
      this.$['zoom-in-button'].show();
      this.$['zoom-out-button'].show();
    }
  },

  hide() {
    if (this.visible_) {
      this.visible_ = false;
      this.$['fit-button'].hide();
      this.$['zoom-in-button'].hide();
      this.$['zoom-out-button'].hide();
    }
  },

  /**
   * Offsets the toolbar position so that it doesn't move if scrollbars appear.
   * @param {!{horizontal: boolean, vertical: boolean}} hasScrollbars
   * @param {number} scrollbarWidth
   */
  shiftForScrollbars(hasScrollbars, scrollbarWidth) {
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
});
