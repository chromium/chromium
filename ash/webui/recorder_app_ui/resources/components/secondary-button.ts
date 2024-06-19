// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-icon-button.js';

import {css, html, LitElement} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';

export class SecondaryButton extends ReactiveLitElement {
  static override shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static override styles = css`
    cra-icon-button {
      --cra-icon-button-container-height: 56px;
      --cra-icon-button-container-width: 72px;
      --cros-icon-button-color-override: var(--cros-sys-secondary);
      --cros-icon-button-icon-size: 24px;

      @container style(--small-viewport: 1) {
        --cra-icon-button-container-height: 40px;
        --cra-icon-button-container-width: 40px;
        --cros-icon-button-icon-size: 20px;
      }
    }
  `;

  override render(): RenderResult {
    return html`<cra-icon-button shape="circle">
      <slot slot="icon" name="icon"></slot>
    </cra-icon-button>`;
  }
}

window.customElements.define('secondary-button', SecondaryButton);

declare global {
  interface HTMLElementTagNameMap {
    'secondary-button': SecondaryButton;
  }
}
