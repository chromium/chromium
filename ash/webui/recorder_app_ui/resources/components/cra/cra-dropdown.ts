// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  Dropdown as CrosDropdown,
} from 'chrome://resources/cros_components/dropdown/dropdown.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

/**
 * A dropdown with ChromeOS specific style.
 */
export class CraDropdown extends CrosDropdown {
  static override styles = [
    CrosDropdown.styles,
    css`
      md-outlined-select {
        /*
         * TODO: b/351271419 - Remove this after upstream have been fixed and
         * upreved.
         */
        --md-menu-container-color: var(--cros-sys-base_elevated);
        --md-menu-item-container-color: var(--cros-sys-base_elevated);

        /* TODO: b/338544996 - Upstream this. */
        width: 100%;
      }
    `,
  ];
}

window.customElements.define('cra-dropdown', CraDropdown);

declare global {
  interface HTMLElementTagNameMap {
    'cra-dropdown': CraDropdown;
  }
}
