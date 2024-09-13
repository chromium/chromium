// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  IconDropdownOption,
} from
  'chrome://resources/cros_components/icon_dropdown/icon-dropdown-option.js';
import {PropertyValues} from 'chrome://resources/mwc/lit/index.js';

export class CraIconDropdownOption extends IconDropdownOption {
  private setAriaChecked(): void {
    this.listItem?.setAttribute('aria-checked', this.checked.toString());
  }

  override firstUpdated(changedProperties: PropertyValues<this>): void {
    super.firstUpdated(changedProperties);
    const role = this.getAttribute('data-role');
    if (role === 'menuitemradio' || role === 'menuitemcheckbox') {
      this.mdMenuItem?.updateComplete.then(() => {
        this.listItem?.setAttribute('role', role);
        this.setAriaChecked();
      });
    }
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('checked')) {
      this.setAriaChecked();
    }
  }
}

window.customElements.define(
  'cra-icon-dropdown-option',
  CraIconDropdownOption,
);

declare global {
  interface HTMLElementTagNameMap {
    'cra-icon-dropdown-option': CraIconDropdownOption;
  }
}
