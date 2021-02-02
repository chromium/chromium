// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// The maximum widths of thumbnails for each layout (px).
// These constants should be kept in sync with `kMaxWidthPortraitPx` and
// `kMaxWidthLandscapePx` in pdf/thumbnail.cc.
/** @type {number} */
const PORTRAIT_WIDTH = 108;
/** @type {number} */
const LANDSCAPE_WIDTH = 140;

/** @type {string} */
export const PAINTED_ATTRIBUTE = 'painted';

export class ViewerThumbnailElement extends PolymerElement {
  static get is() {
    return 'viewer-thumbnail';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      clockwiseRotations: {
        type: Number,
        value: 0,
        observer: 'clockwiseRotationsChanged_',
      },

      isActive: {
        type: Boolean,
        observer: 'isActiveChanged_',
        reflectToAttribute: true,
      },

      pageNumber: Number,
    };
  }

  constructor() {
    super();

    this.addEventListener('keydown', this.onKeydown_);

    FocusOutlineManager.forDocument(document);
  }

  /** @param {!ImageData} imageData */
  set image(imageData) {
    let canvas = this.getCanvas_();
    if (!canvas) {
      canvas = document.createElement('canvas');

      // Prevent copying or saving of the thumbnail image in case the document
      // has restricted access rights.
      canvas.oncontextmenu = e => e.preventDefault();

      this.shadowRoot.querySelector('#thumbnail').appendChild(canvas);
    }

    canvas.width = imageData.width;
    canvas.height = imageData.height;

    this.styleCanvas_();

    const ctx = canvas.getContext('2d');
    ctx.putImageData(imageData, 0, 0);
  }

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
    this.removeAttribute(PAINTED_ATTRIBUTE);
  }

  /** @return {!HTMLElement} */
  getClickTarget() {
    return /** @type {!HTMLElement} */ (
        this.shadowRoot.querySelector('#thumbnail'));
  }

  /** @private */
  clockwiseRotationsChanged_() {
    if (this.getCanvas_()) {
      this.styleCanvas_();
    }
  }

  /**
   * @return {?HTMLCanvasElement}
   * @private
   */
  getCanvas_() {
    return /** @type {?HTMLCanvasElement} */ (
        this.shadowRoot.querySelector('canvas'));
  }

  /**
   * Calculates the CSS size of the thumbnail depending on the rotation, the
   * dimensions of the image data, and the screen resolution. The plugin
   * scales the thumbnail image data by the device to pixel ratio, so that
   * scaling must be taken into account on the UI.
   * @param {boolean} rotated
   * @return {!{width: number, height: number}}
   * @private
   */
  getThumbnailCssSize_(rotated) {
    const canvas = this.getCanvas_();
    const isPortrait = canvas.width < canvas.height !== rotated;
    const orientedWidth = rotated ? canvas.height : canvas.width;
    const orientedHeight = rotated ? canvas.width : canvas.height;

    // Try scaling down such that the width of thumbnail is `PORTRAIT_WIDTH` or
    // `LANDSCAPE_WIDTH`, but never scale up to retain the resolution of the
    // thumbnail.
    const cssWidth = Math.min(
        isPortrait ? PORTRAIT_WIDTH : LANDSCAPE_WIDTH,
        parseInt(orientedWidth / window.devicePixelRatio, 10));
    const scale = cssWidth / orientedWidth;
    const cssHeight = parseInt(orientedHeight * scale, 10);
    return {width: cssWidth, height: cssHeight};
  }

  /**
   * Focuses and scrolls the element into view.
   * The default scroll behavior of focus() acts differently than
   * scrollIntoView(), which is called in isActiveChanged_(). This method
   * unifies the behavior.
   */
  focusAndScroll() {
    this.scrollIntoView({block: 'nearest'});
    this.focus({preventScroll: true});
  }

  /** @return {boolean} */
  isPainted() {
    return this.hasAttribute(PAINTED_ATTRIBUTE);
  }

  setPainted() {
    this.toggleAttribute(PAINTED_ATTRIBUTE, true);
  }

  /** @private */
  isActiveChanged_() {
    if (this.isActive) {
      this.scrollIntoView({block: 'nearest'});
    }
  }

  /** @private */
  focusThumbnailNext_() {
    if (this.nextElementSibling &&
        this.nextElementSibling.matches('viewer-thumbnail')) {
      this.nextElementSibling.focusAndScroll();
    }
  }

  /** @private */
  focusThumbnailPrev_() {
    if (this.previousElementSibling &&
        this.previousElementSibling.matches('viewer-thumbnail')) {
      this.previousElementSibling.focusAndScroll();
    }
  }

  /** @private */
  onClick_() {
    this.dispatchEvent(new CustomEvent('change-page', {
      detail: {page: this.pageNumber - 1, origin: 'thumbnail'},
      bubbles: true,
      composed: true
    }));
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeydown_(e) {
    const keyboardEvent = /** @type {!KeyboardEvent} */ (e);
    switch (keyboardEvent.key) {
      case 'ArrowDown':
        // Prevent default arrow scroll behavior.
        keyboardEvent.preventDefault();
        this.focusThumbnailNext_();
        break;
      case 'ArrowUp':
        // Prevent default arrow scroll behavior.
        keyboardEvent.preventDefault();
        this.focusThumbnailPrev_();
        break;
      case 'Enter':
      case ' ':
        // Prevent default space scroll behavior.
        keyboardEvent.preventDefault();
        this.onClick_();
        break;
    }
  }

  /**
   * Sets the canvas CSS size to maintain the resolution of the thumbnail at any
   * rotation.
   * @private
   */
  styleCanvas_() {
    assert(this.clockwiseRotations >= 0 && this.clockwiseRotations < 4);

    const canvas = this.getCanvas_();
    const div = this.shadowRoot.querySelector('#thumbnail');

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

customElements.define(ViewerThumbnailElement.is, ViewerThumbnailElement);
