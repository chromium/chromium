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

import {setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
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
      isPersonalizationJellyEnabled_: {
        type: Boolean,
        value() {
          return isPersonalizationJellyEnabled();
        },
      },
      ambientModeEnabled_: Boolean,
    };
  }

  private isPersonalizationJellyEnabled_: boolean;
  private ambientModeEnabled_: boolean|null;
  override ariaLabel: string;

  override focus() {
    this.$.toggle.focus();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<ToggleRow['ambientModeEnabled_']>(
        'ambientModeEnabled_', state => state.ambient.ambientModeEnabled);
    this.updateFromStore();
  }

  private getAriaLabel_(): string {
    return this.i18n(
        this.ambientModeEnabled_ ? 'ambientModeOn' : 'ambientModeOff');
  }

  private getToggleRowTitle_(): string {
    return this.getAriaLabel_().toUpperCase();
  }

  private onAmbientModeToggled_(event: Event) {
    const toggleButton = event.currentTarget as CrToggleElement;
    const ambientModeEnabled = toggleButton!.checked;
    setAmbientModeEnabled(
        ambientModeEnabled, getAmbientProvider(), this.getStore());
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'toggle-row': ToggleRow;
  }
}

customElements.define(ToggleRow.is, ToggleRow);
