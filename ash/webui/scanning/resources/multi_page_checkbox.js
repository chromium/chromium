// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'multi-page-checkbox' displays the checkbox for starting a multi-page scan.
 */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const MultiPageCheckboxElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
class MultiPageCheckboxElement extends MultiPageCheckboxElementBase {
  static get is() {
    return 'multi-page-checkbox';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** {boolean} */
      multiPageScanChecked: {
        type: Boolean,
        notify: true,
      },

      /** @type {boolean} */
      disabled: Boolean,
    };
  }

  /** @private */
  onCheckboxClick_() {
    if (this.disabled) {
      return;
    }

    this.multiPageScanChecked = !this.multiPageScanChecked;
  }
}

customElements.define(MultiPageCheckboxElement.is, MultiPageCheckboxElement);
