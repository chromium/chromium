// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './zero_state_overlay.css.js';
import {getHtml} from './zero_state_overlay.html.js';

export interface ZeroStateOverlayElement {
  $: {
    opaqueOverlay: HTMLElement,
  };
}
export class ZeroStateOverlayElement extends CrLitElement {
  static get is() {
    return 'zero-state-overlay';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isFirstLoad: {
        type: Boolean,
        reflect: true,
      },
      isSidePanel: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  // When called, will toggle fading overlay animation in zero state overlay.
  startOverlayAnimation() {
    if (this.currentAnimation_) {
      this.currentAnimation_.cancel();
    }
    this.$.opaqueOverlay.style.display = 'block';
    this.currentAnimation_ = this.$.opaqueOverlay.animate(
        [
          {opacity: 1, display: 'block'},
          {opacity: 0, display: 'block'},
        ],
        {
          duration: 1200,
          delay: 1800,
          easing: 'cubic-bezier(0, 0, 0.58, 1)',
          fill: 'both',
        },
    );

    this.currentAnimation_.onfinish = () => {
      this.currentAnimation_ = null;
      this.$.opaqueOverlay.style.display = 'none';
    };
  }

  accessor isFirstLoad: boolean = false;
  accessor isSidePanel: boolean = false;
  protected currentAnimation_: Animation|null = null;
}
declare global {
  interface HtmlElementTagNameMap {
    'zero-state-overlay': ZeroStateOverlayElement;
  }
}
customElements.define(ZeroStateOverlayElement.is, ZeroStateOverlayElement);
