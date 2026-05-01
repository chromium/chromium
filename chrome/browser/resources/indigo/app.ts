// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './motion_overlay.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

// TODO(b/486887445): Remove the hardcoded duration once there is a way
// to receive the transformed image.
const EXIT_ANIMATION_DELAY_MS: number = 8000;

export interface IndigoImageReplacementAppElement {
  $: {
    image: HTMLImageElement,
  };
}

export class IndigoImageReplacementAppElement extends CrLitElement {
  static get is() {
    return 'indigo-image-replacement-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showOverlay_: {type: Boolean},
      overlayAnimationState_: {type: String},
      imageSrc_: {type: String},
    };
  }

  protected accessor showOverlay_: boolean = false;
  protected accessor overlayAnimationState_: 'entry'|'exit'|'none' = 'none';
  protected accessor imageSrc_: string = '';

  private exitTimeout_: number|null = null;

  override async connectedCallback() {
    super.connectedCallback();
    await this.loadOriginalImage_();
    requestAnimationFrame(async () => {
      await chrome.indigoPrivate.readyToRender();
      this.startAnimation_();
    });
  }

  protected onMotionComplete_() {
    this.showOverlay_ = false;
    if (this.exitTimeout_) {
      window.clearTimeout(this.exitTimeout_);
    }
  }

  private async loadOriginalImage_() {
    const imageData = await chrome.indigoPrivate.getOriginalImage();
    const blob = new Blob([imageData.webpBytes], {type: 'image/webp'});
    this.imageSrc_ = URL.createObjectURL(blob);
    await this.updateComplete;
    await this.$.image.decode();
  }

  private startAnimation_() {
    this.showOverlay_ = true;
    this.overlayAnimationState_ = 'entry';
    this.exitTimeout_ = window.setTimeout(() => {
      this.overlayAnimationState_ = 'exit';
    }, EXIT_ANIMATION_DELAY_MS);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-image-replacement-app': IndigoImageReplacementAppElement;
  }
}

customElements.define(
    IndigoImageReplacementAppElement.is, IndigoImageReplacementAppElement);
