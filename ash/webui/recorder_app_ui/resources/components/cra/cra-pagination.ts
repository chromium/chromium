// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  classMap,
  css,
  html,
  LitElement,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

export class CraPagination extends LitElement {
  static override styles = css`
    :host {
      display: flex;
      flex-direction: row;
      gap: 2px;
    }

    .dot {
      border: 1px solid var(--cros-sys-primary);
      border-radius: var(--border-radius-rounded-with-short-side);
      box-sizing: border-box;
      height: 8px;
      margin: 6px;
      width: 8px;

      &.selected {
        background: var(--cros-sys-primary);
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    dots: {type: Number},
    selected: {type: Number},
  };

  /**
   * The total number of dots.
   */
  dots: number = 0;

  /**
   * The selected dot index starting from 0.
   */
  selected: number = 0;

  override render(): RenderResult {
    return Array.from({length: this.dots}).fill(null).map((_, i) => {
      const classes = {selected: i === this.selected};
      return html`<div class="dot ${classMap(classes)}"></div>`;
    });
  }
}

window.customElements.define('cra-pagination', CraPagination);

declare global {
  interface HTMLElementTagNameMap {
    'cra-pagination': CraPagination;
  }
}
