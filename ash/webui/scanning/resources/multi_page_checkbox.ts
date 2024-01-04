// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import './scan_settings_section.js';
import './strings.m.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './multi_page_checkbox.html.js';

/**
 * @fileoverview
 * 'multi-page-checkbox' displays the checkbox for starting a multi-page scan.
 */

const MultiPageCheckboxElementBase = I18nMixin(PolymerElement);

export class MultiPageCheckboxElement extends MultiPageCheckboxElementBase {
  static get is() {
    return 'multi-page-checkbox' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      multiPageScanChecked: {
        type: Boolean,
        notify: true,
      },

      disabled: Boolean,
    };
  }

  multiPageScanChecked: boolean;
  disabled: boolean;

  private onCheckboxClick(): void {
    if (this.disabled) {
      return;
    }

    this.multiPageScanChecked = !this.multiPageScanChecked;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [MultiPageCheckboxElement.is]: MultiPageCheckboxElement;
  }
}

customElements.define(MultiPageCheckboxElement.is, MultiPageCheckboxElement);
