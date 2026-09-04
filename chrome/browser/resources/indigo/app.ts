// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './motion_overlay.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

type ObjectFit = 'fill'|'contain'|'cover'|'none'|'scale-down';

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
      showWatermark_: {type: Boolean},
      watermarkEnabledString: {type: String, attribute: 'show-watermark'},
    };
  }

  protected accessor showOverlay_: boolean = false;
  protected accessor overlayAnimationState_: 'entry'|'exit'|'none' = 'none';
  protected accessor imageSrc_: string = '';
  protected accessor objectFit_: ObjectFit = 'contain';
  protected accessor showWatermark_: boolean = false;
  protected accessor watermarkEnabledString: string = 'false';

  private invocationId_: number|undefined;
  private isGenerating_: boolean = false;
  private entryAnimationResolve_: (() => void)|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.initialize_();
  }

  protected onMotionComplete_() {
    this.showOverlay_ = false;
    this.overlayAnimationState_ = 'none';
  }

  protected onEntryComplete_() {
    this.entryAnimationResolve_?.();
    this.entryAnimationResolve_ = null;
  }

  private async initialize_() {
    await this.loadOriginalImage_();
    requestAnimationFrame(async () => {
      this.invocationId_ = await chrome.indigoPrivate.readyToRender();
      this.loadReplacementImage_();
      chrome.indigoPrivate.onRegenerateStarted.addListener(() => {
        this.loadReplacementImage_();
      }, {instanceId: this.invocationId_});
    });
  }

  private async loadOriginalImage_() {
    const originalImage = await chrome.indigoPrivate.getOriginalImage();
    if (originalImage.objectFit) {
      this.objectFit_ = originalImage.objectFit === 'object-fit-none' ?
          'none' :
          originalImage.objectFit;
    }
    if (originalImage.value instanceof ArrayBuffer) {
      const blob = new Blob([originalImage.value], {type: 'image/webp'});
      this.imageSrc_ = URL.createObjectURL(blob);
      await this.updateComplete;
      await this.$.image.decode();
    }
  }

  private async loadReplacementImage_() {
    if (this.isGenerating_) {
      return;
    }
    this.isGenerating_ = true;
    const entryAnimationPromise = new Promise<void>(resolve => {
      this.entryAnimationResolve_ = resolve;
    });
    this.startAnimation_();
    try {
      const [imageData] = await Promise.all([
        chrome.indigoPrivate.getReplacementImage(),
        entryAnimationPromise,
      ]);
      if (typeof imageData.value === 'string') {
        const img = new Image();
        img.src = imageData.value;
        await img.decode();

        URL.revokeObjectURL(this.imageSrc_);
        this.imageSrc_ = imageData.value;
        this.objectFit_ = this.computeObjectFitForReplacement_(img);
        this.showWatermark_ = this.watermarkEnabledString === 'true';
        await this.updateComplete;
      }
    } finally {
      this.entryAnimationResolve_ = null;
      this.isGenerating_ = false;
      this.overlayAnimationState_ = 'exit';
    }
  }

  private startAnimation_() {
    this.showOverlay_ = true;
    this.overlayAnimationState_ = 'entry';
  }

  private computeObjectFitForReplacement_(
      image: {naturalWidth: number, naturalHeight: number}): 'contain'|'cover' {
    const {naturalWidth, naturalHeight} = image;
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
