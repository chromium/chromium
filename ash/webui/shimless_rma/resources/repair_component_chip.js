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

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {modifyTabbableElement} from './shimless_rma_util.js';

/**
 * @fileoverview
 * 'repair-component-chip' represents a single component chip that can be marked
 * as replaced.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const RepairComponentChipBase = mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class RepairComponentChip extends RepairComponentChipBase {
  static get is() {
    return 'repair-component-chip';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {boolean} */
      disabled: {
        type: Boolean,
        value: false,
      },

      /** @type {boolean} */
      checked: {
        notify: true,
        reflectToAttribute: true,
        type: Boolean,
        value: false,
      },

      /** @type {string} */
      componentName: {type: String, value: ''},

      /** @type {string} */
      componentIdentifier: {type: String, value: ''},

      /** @type {number} */
      uniqueId: {
        reflectToAttribute: true,
        type: Number,
        value: '',
      },

      /** @type {boolean} */
      isFirstClickableComponent: {
        type: Boolean,
        value: false,
        observer:
            RepairComponentChip.prototype.onIsFirstClickableComponentChanged_,
      },

    };
  }

  /** @protected */
  onComponentButtonClicked_() {
    this.checked = !this.checked;

    // Notify the page that the component chip was clicked, so that the page can
    // put the focus on it.
    this.dispatchEvent(new CustomEvent('click-repair-component-button', {
      bubbles: true,
      composed: true,
      detail: this.uniqueId,
    }));
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

customElements.define(RepairComponentChip.is, RepairComponentChip);
