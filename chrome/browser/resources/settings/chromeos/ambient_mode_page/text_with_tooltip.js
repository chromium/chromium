// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying clamped text, with a tooltip if
 * the text overflows.
 */

import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../settings_shared_css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const TOOLTIP_ANIMATE_IN_DELAY = 500;
const TOOLTIP_ANIMATE_OUT_DURATION = 500;

/** @polymer */
class TextWithTooltipElement extends PolymerElement {
  static get is() {
    return 'text-with-tooltip';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** Text displayed in both div and tooltip.*/
      text: {
        type: String,
        observer: 'textChanged_',
      },

      /** Number of lines to limit to */
      lineClamp: {
        type: String,
        value: '1',
      },

      /** Style class(es) for the text */
      textStyle: {
        type: String,
      },

      /**
       * Whether #textDiv has overflowing content
       * @private
       */
      textOverflowing_: {
        type: Boolean,
        value: false,
      },

      /** Whether the tooltip is visible or animating */
      tooltipIsVisible: {
        type: Boolean,
        value: false,
        notify: true,
      },
    };
  }

  /**
   * Called after property values are set and local DOM is initialized.
   * @override
   */
  ready() {
    super.ready();

    const resizeObserver = new ResizeObserver((mutations) => {
      this.updateTextOverflowing_();
    });
    resizeObserver.observe(this.$.textDiv);

    this.addEventListener('mouseenter', this.onMouseEnter_);
    this.addEventListener('mouseleave', this.onMouseLeave_);
  }

  /**
   * Called whenever the text has changed.
   * @private
   */
  textChanged_(newValue, oldValue) {
    this.updateTextOverflowing_();
  }

  /**
   * @param {Event} event
   * @private
   */
  onMouseEnter_(/** @type {!MouseEvent}*/ event) {
    this.updateTooltipIsVisibleAfterDelay_(true, TOOLTIP_ANIMATE_IN_DELAY);
  }

  /**
   * @param {Event} event
   * @private
   */
  onMouseLeave_(/** @type {!MouseEvent}*/ event) {
    const animationDurationOut = 500;
    this.updateTooltipIsVisibleAfterDelay_(false, TOOLTIP_ANIMATE_OUT_DURATION);
  }

  /**
   * paper-tooltip doesn't have a reliable way of querying it's visibility
   * (animations included). It animates in after a duration after mouseenter.
   * And starts animating out on mouseleave.
   * @param {boolean} visible tooltip visibility.
   * @param {number} delay in ms to apply visibility.
   * @private
   */
  updateTooltipIsVisibleAfterDelay_(visible, delay) {
    const tooltip = this.shadowRoot.querySelector('paper-tooltip');
    if (tooltip) {
      setTimeout(() => this.tooltipIsVisible = visible, delay);
    }
  }

  /**
   * Sets textOverflowing_ based on the #textDiv element.
   * @private
   */
  updateTextOverflowing_() {
    const textElement = this.$.textDiv;
    this.textOverflowing_ = textElement.scrollHeight > textElement.offsetHeight;
  }
}

customElements.define(TextWithTooltipElement.is, TextWithTooltipElement);
