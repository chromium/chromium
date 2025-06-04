// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {GLIF_HEX_COLORS} from './color_utils.js';
import {getCss} from './overlay_border_glow.css.js';
import {getHtml} from './overlay_border_glow.html.js';

/*
 * Element responsible for rendering the border glow as a replacement for the
 * shimmer.
 */
export class OverlayBorderGlowElement extends CrLitElement {
  static get is() {
    return 'overlay-border-glow';
  }

  static override get properties() {
    return {
      isFadingOut: {
        type: Boolean,
        reflect: true,
      },
      isFadingIn: {
        type: Boolean,
        reflect: true,
      },
      selectionOverlayRect: {type: DOMRect},
    };
  }

  // Whether the border glow is fading out.
  private accessor isFadingOut: boolean = false;
  // Whether the border glow is fading in.
  private accessor isFadingIn: boolean = false;
  // The bounding box of the selection overlay.
  private accessor selectionOverlayRect: DOMRect = new DOMRect(0, 0, 0, 0);

  static override get styles() {
    return getCss();
  }

  protected getGradientColorStyles(): string {
    const styles: string[] = [
      `--gradient-blue: ${GLIF_HEX_COLORS.blue}`,
      `--gradient-red: ${GLIF_HEX_COLORS.red}`,
      `--gradient-yellow: ${GLIF_HEX_COLORS.yellow}`,
      `--gradient-green: ${GLIF_HEX_COLORS.green}`,
    ];
    return styles.join(';');
  }

  protected getBoundsStyles(): string {
    /* Height and width must be larger than the diagonal of the viewport,
    in order to prevent gaps at the corners while rotating. */
    const longestSide = Math.max(
        this.selectionOverlayRect.width, this.selectionOverlayRect.height);
    return `width: ${longestSide * 1.5}px; height: ${longestSide * 1.5}px`;
  }

  handleGestureStart() {
    this.isFadingOut = true;
  }

  handlePostSelectionUpdated() {
    this.isFadingOut = true;
  }

  handleClearSelection() {
    this.isFadingOut = false;
    this.isFadingIn = true;
  }

  override render() {
    return getHtml.bind(this)();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'overlay-border-glow': OverlayBorderGlowElement;
  }
}

customElements.define(OverlayBorderGlowElement.is, OverlayBorderGlowElement);

// Register the custom property for the gradient mask opacity middle value.
// Custom properties are ignored by the browser in shadow DOMs, so need to
// register them globally here. Additionally, the property can only by
// registered once per document, so this must be done in the main window, rather
// than in the class itself.
window.CSS.registerProperty({
  name: '--gradient-mask-opacity-middle-val',
  syntax: '<number>',
  inherits: false,
  initialValue: '0',
});
