// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a description text and a toggle button.
 */

import '../../css/common.css.js';
import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';

import {isPersonalizationJellyEnabled} from '../load_time_booleans.js';
import {WithPersonalizationStore} from '../personalization_store.js';

import {getTemplate} from './toggle_row_element.html.js';

export interface ToggleRow {
  $: {toggle: CrToggleElement};
}

export class ToggleRow extends WithPersonalizationStore {
  static get is() {
    return 'toggle-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      checked:
          {type: Boolean, value: false, notify: true, reflectToAttribute: true},

      isPersonalizationJellyEnabled_: {
        type: Boolean,
        value() {
          return isPersonalizationJellyEnabled();
        },
      },
    };
  }

  checked: boolean;
  private isPersonalizationJellyEnabled_: boolean;
  override ariaLabel: string;

  override focus() {
    this.$.toggle.focus();
  }

  private getAriaLabel_(): string {
    return this.i18n(this.checked ? 'ambientModeOn' : 'ambientModeOff');
  }

  private getToggleRowTitle_(): string {
    return this.getAriaLabel_().toUpperCase();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toggle-row': ToggleRow;
  }
}

customElements.define(ToggleRow.is, ToggleRow);
