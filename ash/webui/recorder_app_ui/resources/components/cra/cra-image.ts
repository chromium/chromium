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
 * A SVG image in the bundled images referred by the name.
 *
 * Image size is controlled by the SVG itself and without a default fill
 * color.
 */
export class CraImage extends LitElement {
  static override styles = css`
    :host {
      display: block;
      height: fit-content;
      width: fit-content;
    }

    svg {
      display: block;
    }
  `;

  static override properties: PropertyDeclarations = {
    name: {type: String},
  };

  name: string|null = null;

  override connectedCallback(): void {
    super.connectedCallback();
    if (!this.hasAttribute('aria-hidden')) {
      // Set default of aria-hidden to true since the illustration is typically
      // accompanied by other text descriptions.
      this.setAttribute('aria-hidden', 'true');
    }
  }

  override render(): RenderResult {
    if (this.name === null) {
      return null;
    }
    return assertExists(images.get(this.name));
  }
}

window.customElements.define('cra-image', CraImage);

declare global {
  interface HTMLElementTagNameMap {
    'cra-image': CraImage;
  }
}
