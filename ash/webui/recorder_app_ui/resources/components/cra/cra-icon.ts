// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {images} from '/images/images.js';
import {
  css,
  LitElement,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {assertExists} from '../../core/utils/assert.js';

/**
 * A SVG icon in the bundled images referred by the name.
 *
 * The size of the icon is controlled by the parent, and the SVG fill is set to
 * current color.
 */
export class CraIcon extends LitElement {
  static override styles = css`
    :host {
      display: block;
    }

    svg {
      display: block;
      fill: currentcolor;
      height: 100%;
      stroke-width: 0;
      stroke: currentcolor;
      width: 100%;
    }
  `;

  static override properties: PropertyDeclarations = {
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

  override render(): RenderResult {
    if (this.name === null) {
      return null;
    }
    return assertExists(images.get(`icons/${this.name}`));
  }
}

window.customElements.define('cra-icon', CraIcon);

declare global {
  interface HTMLElementTagNameMap {
    'cra-icon': CraIcon;
  }
}
