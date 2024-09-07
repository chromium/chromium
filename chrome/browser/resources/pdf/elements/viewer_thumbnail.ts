// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ChangePageOrigin} from './viewer_bookmark.js';
import {getCss} from './viewer_thumbnail.css.js';
import {getHtml} from './viewer_thumbnail.html.js';

// The maximum widths of thumbnails for each layout (px).
// These constants should be kept in sync with `kMaxWidthPortraitPx` and
// `kMaxWidthLandscapePx` in pdf/thumbnail.cc.
const PORTRAIT_WIDTH: number = 108;

const LANDSCAPE_WIDTH: number = 140;

const PDF_CANVAS_ID: string = 'pdf-canvas';

// <if expr="enable_pdf_ink2">
const INK2_CANVAS_ID: string = 'ink2-canvas';
// </if>

export const PAINTED_ATTRIBUTE: string = 'painted';

export interface ViewerThumbnailElement {
  $: {
    thumbnail: HTMLElement,
  };
}

export class ViewerThumbnailElement extends CrLitElement {
  static get is() {
    return 'viewer-thumbnail';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      clockwiseRotations: {type: Number},

      isActive: {
        type: Boolean,
        reflect: true,
      },

      pageNumber: {type: Number},
    };
  }

  clockwiseRotations: number = 0;
  isActive: boolean = true;
  pageNumber: number = 0;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('clockwiseRotations')) {
      const canvas = this.getCanvas_();
      if (canvas) {
        this.styleCanvas_(canvas);
      }
      // <if expr="enable_pdf_ink2">
      const ink2Canvas = this.getInk2Canvas_();
      if (ink2Canvas) {
        this.styleCanvas_(ink2Canvas);
      }
      // </if>
    }

    if (changedProperties.has('isActive') && this.isActive) {
      this.scrollIntoView({block: 'nearest'});
    }
  }

  private createCanvasHelper_(id: string) {
    const canvas = document.createElement('canvas');
    canvas.id = id;

    // Prevent copying or saving of the thumbnail image in case the document
    // has restricted access rights.
    canvas.oncontextmenu = e => e.preventDefault();

    return canvas;
  }

  private setImageHelper_(canvas: HTMLCanvasElement, imageData: ImageData) {
    canvas.width = imageData.width;
    canvas.height = imageData.height;

    this.styleCanvas_(canvas);

    const ctx = canvas.getContext('2d')!;
    ctx.putImageData(imageData, 0, 0);
  }

  set image(imageData: ImageData) {
    let canvas = this.getCanvas_();
    if (!canvas) {
      canvas = this.createCanvasHelper_(PDF_CANVAS_ID);
      const canvasContainer =
          this.$.thumbnail.querySelector('#canvas-container')!;
      canvasContainer.appendChild(canvas);
    }

    this.setImageHelper_(canvas, imageData);
  }

  // <if expr="enable_pdf_ink2">
  set ink2Image(imageData: ImageData) {
    let canvas = this.getInk2Canvas_();
    if (!canvas) {
      canvas = this.createCanvasHelper_(INK2_CANVAS_ID);
      const canvasContainer =
          this.$.thumbnail.querySelector('#canvas-container')!;
      canvasContainer.insertBefore(canvas, canvasContainer.firstChild);
    }

    this.setImageHelper_(canvas, imageData);
  }
  // </if>

  clearImage() {
    if (!this.isPainted()) {
      return;
    }

    // `canvas` can be `null` in tests because `image` is set only in response
    // to the plugin.
    const canvas = this.getCanvas_();
    if (canvas) {
      canvas.remove();
    }
    // <if expr="enable_pdf_ink2">
    const ink2Canvas = this.getInk2Canvas_();
    if (ink2Canvas) {
      ink2Canvas.remove();
    }
    // </if>
    this.removeAttribute(PAINTED_ATTRIBUTE);
  }

  getClickTarget(): HTMLElement {
    return this.$.thumbnail;
  }

  private getCanvas_(): HTMLCanvasElement|null {
    return this.shadowRoot!.querySelector('#' + PDF_CANVAS_ID);
  }

  // <if expr="enable_pdf_ink2">
  private getInk2Canvas_(): HTMLCanvasElement|null {
    return this.shadowRoot!.querySelector('#' + INK2_CANVAS_ID);
  }
  // </if>

  /**
   * Calculates the CSS size of the thumbnail depending on the rotation, the
   * dimensions of the image data, and the screen resolution. The plugin
   * scales the thumbnail image data by the device to pixel ratio, so that
   * scaling must be taken into account on the UI.
   */
  private getThumbnailCssSize_(rotated: boolean):
      {width: number, height: number} {
    const canvas = this.getCanvas_()!;
    const isPortrait = canvas.width < canvas.height !== rotated;
    const orientedWidth = rotated ? canvas.height : canvas.width;
    const orientedHeight = rotated ? canvas.width : canvas.height;

    // Try scaling down such that the width of thumbnail is `PORTRAIT_WIDTH` or
    // `LANDSCAPE_WIDTH`, but never scale up to retain the resolution of the
    // thumbnail.
    const cssWidth = Math.min(
        isPortrait ? PORTRAIT_WIDTH : LANDSCAPE_WIDTH,
        Math.trunc(orientedWidth / window.devicePixelRatio));
    const scale = cssWidth / orientedWidth;
    const cssHeight = Math.trunc(orientedHeight * scale);
    return {width: cssWidth, height: cssHeight};
  }

  /**
   * Focuses and scrolls the element into view.
   * The default scroll behavior of focus() acts differently than
   * scrollIntoView(), which is called in updated(). This method
   * unifies the behavior.
   */
  focusAndScroll() {
    this.scrollIntoView({block: 'nearest'});
    this.focus({preventScroll: true});
  }

  isPainted(): boolean {
    return this.hasAttribute(PAINTED_ATTRIBUTE);
  }

  setPainted() {
    this.toggleAttribute(PAINTED_ATTRIBUTE, true);
  }

  protected onClick_() {
    this.fire(
        'change-page',
        {page: this.pageNumber - 1, origin: ChangePageOrigin.THUMBNAIL});
  }

  /**
   * Sets the canvas CSS size to maintain the resolution of the thumbnail at any
   * rotation.
   */
  private styleCanvas_(canvas: HTMLCanvasElement) {
    assert(this.clockwiseRotations >= 0 && this.clockwiseRotations < 4);

    const div = this.shadowRoot!.querySelector<HTMLElement>('#thumbnail')!;

    const degreesRotated = this.clockwiseRotations * 90;
    canvas.style.transform = `rotate(${degreesRotated}deg)`;

    // For the purposes of determining the dimensions, a rotation of 180deg is
    // not rotated.
    const rotated = this.clockwiseRotations % 2 !== 0;
    const cssSize = this.getThumbnailCssSize_(rotated);
    div.style.width = `${cssSize.width}px`;
    div.style.height = `${cssSize.height}px`;

    // When rotated, the canvas's height becomes the parent div's width and vice
    // versa.
    canvas.style.width = `${rotated ? cssSize.height : cssSize.width}px`;
    canvas.style.height = `${rotated ? cssSize.width : cssSize.height}px`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-thumbnail': ViewerThumbnailElement;
  }
}

customElements.define(ViewerThumbnailElement.is, ViewerThumbnailElement);
