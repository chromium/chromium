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
    };
  }

  // Whether the border glow is fading out.
  private accessor isFadingOut: boolean = false;
  // Whether the border glow is fading in.
  private accessor isFadingIn: boolean = false;

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

  handleGestureStart() {
    this.isFadingOut = true;
  }

  /* TODO(crbug.com/419035304): Trigger this when the CSB thumbnail is removed.
   */
  handleRemoveSearchboxThumbnail() {
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
