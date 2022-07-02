// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shimless_rma_fonts_css.js';
import './shimless_rma_shared_css.js';
import './icons.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
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

      /** @type {boolean} */
      isFirstClickableComponent: {
        type: Boolean,
        value: false,
        observer: 'onIsFirstClickableComponentChanged_',
      },

    };
  }

  /** @protected */
  onComponentButtonClicked_() {
    this.checked = !this.checked;
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
}

customElements.define(RepairComponentChip.is, RepairComponentChip);
