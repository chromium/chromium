// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Menu} from 'chrome://resources/cros_components/menu/menu.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

export class CraMenu extends Menu {
  static override styles = [
    Menu.styles,
    // TODO: b/338544996 - Remove this component once the fix is submitted back
    // to upstream.
    css`
      :host {
        --cros-bg-color-elevation-3: var(--cros-sys-base_elevated);
      }
    `,
  ];

  toggle(): void {
    if (this.open) {
      this.close();
    } else {
      this.show();
    }
  }
}

window.customElements.define('cra-menu', CraMenu);

declare global {
  interface HTMLElementTagNameMap {
    'cra-menu': CraMenu;
  }
}
