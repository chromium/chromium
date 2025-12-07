// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  css,
  html,
  LitElement,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {DEFAULT_STYLE} from '../styles.js';

export class SuperResLoadingIndicator extends LitElement {
  static override styles = [
    DEFAULT_STYLE,
    css`
      .spin-start circle {
        animation: loading 0.5s ease-out forwards;
      }

      @keyframes loading {
        from {
          stroke-dashoffset: 315;
        }
        to {
          stroke-dashoffset: 95;
        }
      }

      .spin-complete circle {
        animation: finish 2s ease-out forwards;
      }

      @keyframes finish {
        0% {
          stroke-dashoffset: 95;
        }
        20% {
          stroke-dashoffset: 0;
        }
        25% {
          stroke-opacity: 1;
        }
        40% {
          stroke-opacity: 0;
        }
        55% {
          stroke-opacity: 1;
        }
        70% {
          stroke-opacity: 0;
        }
        85% {
          stroke-opacity: 1;
        }
        100% {
          stroke-opacity: 0;
        }
      }
    `,
  ];

  static override properties: PropertyDeclarations = {
    photoProcessing: {type: Boolean},
  };

  /**
   * Whether the photo processing is ongoing.
   */
  photoProcessing = false;

  override render(): RenderResult {
    return html`
    <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"
        class=${this.photoProcessing ? 'spin-start' : 'spin-complete'}>
      <circle stroke-linecap="round" cx="50" cy="50" r="48"
          stroke="var(--cros-sys-on_primary_container)" stroke-width="5"
          fill="none" stroke-dasharray="315"
          transform="rotate(-90,50,50)" />
    </svg>
    `;
  }
}

window.customElements.define(
    'super-res-loading-indicator', SuperResLoadingIndicator);

declare global {
  interface HTMLElementTagNameMap {
    'super-res-loading-indicator': SuperResLoadingIndicator;
  }
}
