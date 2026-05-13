// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './motion_overlay.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

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
      objectFit_: {type: String},
    };
  }

  protected accessor showOverlay_: boolean = false;
  protected accessor overlayAnimationState_: 'entry'|'exit'|'none' = 'none';
  protected accessor imageSrc_: string = '';
  protected accessor objectFit_: 'contain'|'cover' = 'contain';

  override async connectedCallback() {
    super.connectedCallback();
    await this.loadOriginalImage_();
    requestAnimationFrame(async () => {
      await chrome.indigoPrivate.readyToRender();
      this.startAnimation_();
      this.getReplacementImage_();
    });
  }

  protected onMotionComplete_() {
    this.showOverlay_ = false;
  }

  private async loadOriginalImage_() {
    const imageData = await chrome.indigoPrivate.getOriginalImage();
    if (imageData.value instanceof ArrayBuffer) {
      const blob = new Blob([imageData.value], {type: 'image/webp'});
      await this.updateAndDecodeImage_(URL.createObjectURL(blob));
    }
  }

  private startAnimation_() {
    this.showOverlay_ = true;
    this.overlayAnimationState_ = 'entry';
  }

  private async getReplacementImage_() {
    const imageData = await chrome.indigoPrivate.getReplacementImage();
    if (typeof imageData.value === 'string') {
      URL.revokeObjectURL(this.imageSrc_);
      await this.updateAndDecodeImage_(imageData.value);
      this.objectFit_ = this.computeObjectFitForReplacement_();
      this.overlayAnimationState_ = 'exit';
    }
  }

  private async updateAndDecodeImage_(src: string) {
    this.imageSrc_ = src;
    await this.updateComplete;
    await this.$.image.decode();
  }

  private computeObjectFitForReplacement_(): 'contain'|'cover' {
    const {naturalWidth, naturalHeight} = this.$.image;
    if (naturalWidth !== naturalHeight) {
      return 'contain';
    }
    const {clientWidth, clientHeight} = document.documentElement;
    if (clientWidth === 0 || clientHeight === 0) {
      return 'contain';
    }
    const aspectRatio = clientWidth / clientHeight;
    return 0.5 <= aspectRatio && aspectRatio <= 1.0 ? 'cover' : 'contain';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'indigo-image-replacement-app': IndigoImageReplacementAppElement;
  }
}

customElements.define(
    IndigoImageReplacementAppElement.is, IndigoImageReplacementAppElement);
