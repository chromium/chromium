// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a description text and a toggle button.
 */

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';

import {CrToggleElement} from 'chrome://resources/cr_elements/cr_toggle/cr_toggle.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {WithPersonalizationStore} from '../personalization_store.js';

export interface ToggleRow {
  $: {toggle: CrToggleElement}
}

export class ToggleRow extends WithPersonalizationStore {
  static get is() {
    return 'toggle-row';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      checked:
          {type: Boolean, value: false, notify: true, reflectToAttribute: true},
    };
  }

  checked: boolean;
  override ariaLabel: string;

  private getAriaLabel_(): string {
    return this.i18n(this.checked ? 'ambientModeOn' : 'ambientModeOff');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toggle-row': ToggleRow;
  }
}

customElements.define(ToggleRow.is, ToggleRow);
