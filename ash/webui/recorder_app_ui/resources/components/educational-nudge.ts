// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {
  css,
  html,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';

// Educational nudges inform users about the feature/behavior.
export class EducationalNudge extends ReactiveLitElement {
  static override styles = css`
    :host {
      align-items: center;
      display: flex;
    }

    div {
      background: var(--cros-sys-primary);
      border-radius: var(--border-radius-rounded-with-short-side);
      color: var(--cros-sys-on_primary);
      font: var(--cros-body-1-font);
    }

    .text {
      padding: 8px 16px;
      width: max-content;
    }

    .dot {
      height: 8px;
      margin: 4px;
      width: 8px;
    }
  `;

  override render(): RenderResult {
    return html`
      <div class="text"><slot></slot></div>
      <div class="dot"></div>
    `;
  }
}

window.customElements.define('educational-nudge', EducationalNudge);

declare global {
  interface HTMLElementTagNameMap {
    'educational-nudge': EducationalNudge;
  }
}
