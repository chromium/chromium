// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-icon-button.js';

import {css} from 'chrome://resources/mwc/lit/index.js';

import {CraIconButton} from './cra/cra-icon-button.js';

export class SecondaryButton extends CraIconButton {
  static override styles = [
    CraIconButton.styles,
    css`
      :host {
        --cra-icon-button-container-height: 56px;
        --cra-icon-button-container-width: 72px;
        --cros-icon-button-color-override: var(--cros-sys-on-surface);
        --cros-icon-button-icon-size: 24px;

        @container style(--small-viewport: 1) {
          --cra-icon-button-container-height: 40px;
          --cra-icon-button-container-width: 40px;
          --cros-icon-button-icon-size: 20px;
        }
      }
    `,
  ];

  constructor() {
    super();
    this.shape = 'circle';
  }
}

window.customElements.define('secondary-button', SecondaryButton);

declare global {
  interface HTMLElementTagNameMap {
    'secondary-button': SecondaryButton;
  }
}
