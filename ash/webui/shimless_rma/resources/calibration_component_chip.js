// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_shared.css.js';
import './icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './calibration_component_chip.html.js';
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
    return getTemplate();
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
                      .onIsFirstClickableComponentChanged,
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
  onComponentButtonClicked() {
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
    this.onComponentButtonClicked();
  }

  /**
   * Show the checked icon for disabled calibration components because if it's
   * disabled, that means it alerady passed calibration.
   * @return {boolean}
   * @protected
   */
  shouldShowCheckIcon() {
    return this.checked || this.disabled;
  }

  /** @private */
  onIsFirstClickableComponentChanged() {
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
  isAriaPressed() {
    return this.checked.toString();
  }
}

customElements.define(
    CalibrationComponentChipElement.is, CalibrationComponentChipElement);
