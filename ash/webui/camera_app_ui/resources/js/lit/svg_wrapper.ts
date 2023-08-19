// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(pihsun): Fix these after CL:4355362 is relanded and CL:4371823 is
// landed.
import {
  css,
  LitElement,
} from 'chrome://resources/mwc/lit/index.js';

import {assertExists} from '../assert.js';
import {preloadedImages} from '../preload_images.js';

export class SvgWrapper extends LitElement {
  static override styles = css`
    :host {
      /* Most common default color for icons. */
      color: var(--cros-sys-on_surface);
      display: block;
      margin: auto;
      height: fit-content;
      width: fit-content;
    }
    svg {
      display: block;
      fill: currentColor;
      stroke: currentColor;
      stroke-width: 0;
    }
  `;

  static override properties = {
    name: {type: String},
  };

  name: string|null = null;

  override connectedCallback(): void {
    super.connectedCallback();
    if (!this.hasAttribute('aria-hidden')) {
      // Set default of aria-hidden to true since the parent element of the SVG
      // is typically a button and would handle a11y instead of the SVG itself.
      this.setAttribute('aria-hidden', 'true');
    }
  }

  override render(): unknown {
    if (this.name === null) {
      return null;
    }
    return assertExists(preloadedImages.get(this.name));
  }
}

window.customElements.define('svg-wrapper', SvgWrapper);

declare global {
  interface HTMLElementTagNameMap {
    /* eslint-disable-next-line @typescript-eslint/naming-convention */
    'svg-wrapper': SvgWrapper;
  }
}
