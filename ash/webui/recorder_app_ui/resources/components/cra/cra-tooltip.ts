// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO: b/338544996 - This is an implementation of the cros-tooltip component
// that is not available in cros-component. Upstream this.

import {css, html, LitElement} from 'chrome://resources/mwc/lit/index.js';

export class CraTooltip extends LitElement {
  static override styles = css`
    :host {
      background-color: var(--cros-sys-on_surface);
      border-radius: 6px;
      box-sizing: border-box;
      color: var(--cros-sys-inverse_on_surface);
      display: block;
      font: var(--cros-annotation-1-font);
      margin: 4px 0 0;
      max-width: 296px;
      padding: 5px 8px;
      position: absolute;
      position-area: bottom span-all;
      position-try: bottom span-right,
        bottom span-left, top,
        top span-right, top span-left;
    }

    span {
      -webkit-box-orient: vertical;
      display: -webkit-box;
      -webkit-line-clamp: 3;
      overflow: hidden;
      text-overflow: ellipsis;
    }
  `;

  override render(): RenderResult {
    return html`<span><slot></slot></span>`;
  }
}

window.customElements.define('cra-tooltip', CraTooltip);

declare global {
  interface HTMLElementTagNameMap {
    'cra-tooltip': CraTooltip;
  }
}
