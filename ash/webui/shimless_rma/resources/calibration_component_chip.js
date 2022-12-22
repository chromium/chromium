// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_fonts_css.js';
import './shimless_rma_shared_css.js';
import './icons.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {modifyTabbableElement} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'calibration-component-chip' represents a single component chip that reports
 * status of last calibration attempt and can be marked to skip.
 */

/** @polymer */
export class CalibrationComponentChipElement extends PolymerElement {
  static get is() {
    return 'calibration-component-chip';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {boolean} */
      checked: {
        notify: true,
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /** @type {boolean} */
      failed: {type: Boolean, value: false},

      /** @type {string} */
      componentName: {type: String, value: ''},

      /** @type {boolean} */
      disabled: {
        type: Boolean,
        value: false,
      },

      /** @type {boolean} */
      isFirstClickableComponent: {
        type: Boolean,
        value: false,
        observer: CalibrationComponentChipElement.prototype
                      .onIsFirstClickableComponentChanged_,
      },

      /** @type {number} */
      uniqueId: {
        reflectToAttribute: true,
        type: Number,
        value: '',
      },
    };
  }

  /** @protected */
  onComponentButtonClicked_() {
    this.checked = !this.checked;

    // Notify the page that the component chip was clicked, so that the page can
    // put the focus on it.
    this.dispatchEvent(new CustomEvent('click-calibration-component-button', {
      bubbles: true,
      composed: true,
      detail: this.uniqueId,
    }));
  }

  click() {
    this.onComponentButtonClicked_();
  }

  /**
   * Show the checked icon for disabled calibration components because if it's
   * disabled, that means it alerady passed calibration.
   * @return {boolean}
   * @protected
   */
  shouldShowCheckIcon_() {
    return this.checked || this.disabled;
  }

  /** @private */
  onIsFirstClickableComponentChanged_() {
    // Tab should go to the first non-disabled component in the list,
    // not individual component.
    modifyTabbableElement(
        /** @type {!HTMLElement} */ (
            this.shadowRoot.querySelector('#componentButton')),
        this.isFirstClickableComponent);
  }

  /**
   * @return {string}
   * @protected
   */
  isAriaPressed_() {
    return this.checked.toString();
  }
}

customElements.define(
    CalibrationComponentChipElement.is, CalibrationComponentChipElement);
