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
import * as dom from '../dom.js';
import {preloadedImages} from '../preload_images.js';

const loadedSvgs = new Map<string, SVGElement>();

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
    }
  `;

  static override properties = {
    name: {type: String},
  };

  name = null;

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
    return assertExists(loadedSvgs.get(this.name)).cloneNode(true);
  }
}

/**
 * Loads all svg images, and define the <svg-wrapper> element.
 *
 * This only needs to be called once at startup.
 */
export function loadSvgImages(): void {
  for (const [imageName, image] of Object.entries(preloadedImages)) {
    const container = document.createElement('div');
    container.innerHTML = image;
    const svg = assertExists(container.querySelector('svg'));
    loadedSvgs.set(imageName, svg);
  }

  for (const el of dom.getAll('[data-svg]', HTMLElement)) {
    const imageName = assertExists(el.dataset['svg']);
    const svg = document.createElement('svg-wrapper');
    svg.setAttribute('name', imageName);
    el.appendChild(svg);
  }

  // This needs to be called after svg are loaded, so the SvgWrapper will
  // always find the svg.
  // JavaScript code that want to show svg should use <svg-wrapper> directly
  // instead of using data-svg attribute.
  window.customElements.define('svg-wrapper', SvgWrapper);
}

declare global {
  interface HTMLElementTagNameMap {
    /* eslint-disable-next-line @typescript-eslint/naming-convention */
    'svg-wrapper': SvgWrapper;
  }
}
