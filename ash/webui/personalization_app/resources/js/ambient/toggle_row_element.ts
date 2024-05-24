// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays a description text and a toggle button.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';

import {CrToggleElement} from 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';

import {WithPersonalizationStore} from '../personalization_store.js';

import {setAmbientModeEnabled} from './ambient_controller.js';
import {getAmbientProvider} from './ambient_interface_provider.js';
import {getTemplate} from './toggle_row_element.html.js';

export interface ToggleRowElement {
  $: {toggle: CrToggleElement};
}

export class ToggleRowElement extends WithPersonalizationStore {
  static get is() {
    return 'toggle-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ambientModeEnabled_: Boolean,
    };
  }

  private ambientModeEnabled_: boolean|null;
  override ariaLabel: string;

  override focus() {
    this.$.toggle.focus();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<ToggleRowElement['ambientModeEnabled_']>(
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
    'toggle-row': ToggleRowElement;
  }
}

customElements.define(ToggleRowElement.is, ToggleRowElement);
