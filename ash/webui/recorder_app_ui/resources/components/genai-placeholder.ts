// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, CSSResultGroup, html} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';

export class GenaiPlaceholder extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    #container {
      align-items: flex-start;
      display: flex;
      flex-flow: column;
      gap: 8px;
      position: relative;
      z-index: 0;
    }

    .line {
      border-radius: var(--border-radius-rounded-with-short-side);
      height: 14px;
      mask: linear-gradient(#000 0 0);
      width: 100%;

      /*
       * We don't use background directly on .line, so the first line will have
       * the same width and "speed" as all other lines.
       */
      &::before {
        animation: 1200ms linear infinite gradient;
        background: left / 200% 100% repeat
          linear-gradient(
            91deg,
            var(--cros-sys-muted),
            var(--cros-sys-complement) 50%,
            var(--cros-sys-muted) 100%
          );
        content: "";
        inset: 0;
        position: absolute;
        z-index: -1;
      }
    }

    #line-1 {
      animation: 1100ms cubic-bezier(0.4, 0, 0, 1) both width;
      max-width: 100%;
      width: 164px;
    }

    /* TODO: b/336963138 - Change these delays according to spec. */
    #line-2 {
      animation: 668ms 250ms cubic-bezier(0.4, 0, 0, 1) both width;

      &::before {
        animation-delay: 67ms;
      }
    }

    #line-3 {
      animation: 668ms 517ms cubic-bezier(0.4, 0, 0, 1) both width;

      &::before {
        animation-delay: 133ms;
      }
    }

    #line-4 {
      animation: 668ms 717ms cubic-bezier(0.4, 0, 0, 1) both width;

      &::before {
        animation-delay: 200ms;
      }
    }

    @keyframes gradient {
      to {
        background-position: -200% 0%;
      }
    }

    @keyframes width {
      from {
        width: 0;
      }
    }
  `;

  override render(): RenderResult {
    // TODO(pihsun): Consider if we should support setting number of lines via
    // property.
    return html`<div id="container">
      <div class="line" id="line-1"></div>
      <div class="line" id="line-2"></div>
      <div class="line" id="line-3"></div>
      <div class="line" id="line-4" part="line-4"></div>
    </div>`;
  }
}

window.customElements.define('genai-placeholder', GenaiPlaceholder);

declare global {
  interface HTMLElementTagNameMap {
    'genai-placeholder': GenaiPlaceholder;
  }
}
