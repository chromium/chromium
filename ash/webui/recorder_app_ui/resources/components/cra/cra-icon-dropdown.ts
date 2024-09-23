// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  IconDropdown,
} from 'chrome://resources/cros_components/icon_dropdown/icon-dropdown.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

export class CraIconDropdown extends IconDropdown {
  static override styles = [
    IconDropdown.styles,
    // Copy some style from the cros-icon-dropdown to make the size responsive.
    // The following style is same as the :host([size="large"]) rule in
    // cros-icon-dropdown.
    css`
      :host([shape]) {
        @container style(--small-viewport: 0) {
          --_button-container-height: 56px;
          --_button-container-width: 72px;
          --_button-icon-size: 24px;
          --_button-icon-padding-inline: 18px 15px;
        }
      }
    `,
  ];
}

window.customElements.define('cra-icon-dropdown', CraIconDropdown);

declare global {
  interface HTMLElementTagNameMap {
    'cra-icon-dropdown': CraIconDropdown;
  }
}
