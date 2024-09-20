// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, html, LitElement} from 'chrome://resources/mwc/lit/index.js';

/**
 * Component containing text to be spoken by the screen reader.
 */
export class SpokenMessage extends LitElement {
  static override styles = css`
    :host {
      border-width: 0;
      clip: rect(0, 0, 0, 0);
      height: 1px;
      margin: -1px;
      opacity: 0;
      overflow: hidden;
      padding: 0;
      pointer-events: none;
      position: absolute;
      user-select: none;
      white-space: nowrap;
      width: 1px;
    }
  `;

  override render(): RenderResult {
    return html`<slot></slot>`;
  }
}

window.customElements.define('spoken-message', SpokenMessage);

declare global {
  interface HTMLElementTagNameMap {
    'spoken-message': SpokenMessage;
  }
}
