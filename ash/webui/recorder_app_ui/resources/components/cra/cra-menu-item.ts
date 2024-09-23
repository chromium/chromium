// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MenuItem} from 'chrome://resources/cros_components/menu/menu_item.js';
import {PropertyValues} from 'chrome://resources/mwc/lit/index.js';

export class CraMenuItem extends MenuItem {
  // TODO(pihsun): Remove this once the upstream fix is merged and pulled in
  // Chromium.
  override set switchSelected(value: boolean) {
    const crosSwitch = this.renderRoot?.querySelector('cros-switch');
    if (!crosSwitch) {
      this.missedPropertySets.switchSelected = value;
    } else {
      crosSwitch.selected = value;
    }
  }

  override get switchSelected(): boolean {
    return (
      this.renderRoot?.querySelector('cros-switch')?.selected ??
      this.missedPropertySets.switchSelected ?? false
    );
  }

  private setAriaChecked(): void {
    this.listItem?.setAttribute('aria-checked', this.checked.toString());
  }

  override firstUpdated(changedProperties: PropertyValues): void {
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

window.customElements.define('cra-menu-item', CraMenuItem);

declare global {
  interface HTMLElementTagNameMap {
    'cra-menu-item': CraMenuItem;
  }
}
