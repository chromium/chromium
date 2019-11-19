// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Namespace for Camera view.
 */
cca.views.camera = cca.views.camera || {};

/**
 * Creates a controller for reviewing intent result in Camera view.
 */
cca.views.camera.ReviewResult = class {
  /**
   * @public
   */
  constructor() {
    /**
     * @const {!HTMLImageElement}
     * @private
     */
    this.reviewPhotoResult_ = /** @type {!HTMLImageElement} */ (
        document.querySelector('#review-photo-result'));

    /**
     * @const {!HTMLVideoElement}
     * @private
     */
    this.reviewVideoResult_ = /** @type {!HTMLVideoElement} */ (
        document.querySelector('#review-video-result'));

    /**
     * @const {!HTMLButtonElement}
     * @private
     */
    this.confirmResultButton_ =
        /** @type {!HTMLButtonElement} */ (
            document.querySelector('#confirm-result'));

    /**
     * @const {!HTMLButtonElement}
     * @private
     */
    this.cancelResultButton_ =
        /** @type {!HTMLButtonElement} */ (
            document.querySelector('#cancel-result'));

    /**
     * @const {!HTMLButtonElement}
     * @private
     */
    this.playResultVideoButton_ =
        /** @type {!HTMLButtonElement} */ (
            document.querySelector('#play-result-video'));

    /**
     * Function resolving open result call called with whether user confirms
     * after reviewing intent result.
     * @type {?function(boolean)}
     * @private
     */
    this.resolveOpen_ = null;

    this.reviewVideoResult_.onended = () => {
      this.reviewVideoResult_.currentTime = 0;
      cca.state.set('playing-result-video', false);
    };

    this.confirmResultButton_.addEventListener(
        'click', () => this.close_(true));
    this.cancelResultButton_.addEventListener(
        'click', () => this.close_(false));
    this.playResultVideoButton_.addEventListener(
        'click', () => this.playResultVideo_());
  }

  /**
   * Starts playing result video.
   * @private
   */
  playResultVideo_() {
    if (cca.state.get('playing-result-video')) {
      return;
    }
    cca.state.set('playing-result-video', true);
    this.reviewVideoResult_.play();
  }

  /**
   * Closes review result UI and resolves its open promise with whether user
   * confirms after reviewing the result.
   * @param {boolean} confirmed
   * @private
   */
  close_(confirmed) {
    if (this.resolveOpen_ === null) {
      console.error('Close review result with no unresolved open.');
      return;
    }
    const resolve = this.resolveOpen_;
    this.resolveOpen_ = null;
    cca.state.set('review-result', false);
    cca.state.set('review-photo-result', false);
    cca.state.set('review-video-result', false);
    cca.state.set('playing-result-video', false);
    this.reviewPhotoResult_.src = '';
    this.reviewVideoResult_.src = '';
    resolve(confirmed);
  }

  /**
   * Opens photo result blob and shows photo on review result UI.
   * @param {!Blob} blob Photo result blob.
   * @return {!Promise<boolean>} Promise resolved with whether user confirms
   *     with the photo result.
   */
  async openPhoto(blob) {
    const img = await cca.util.blobToImage(blob);
    this.reviewPhotoResult_.src = img.src;
    cca.state.set('review-photo-result', true);
    cca.state.set('review-result', true);
    this.confirmResultButton_.focus();

    return new Promise((resolve) => {
      this.resolveOpen_ = resolve;
    });
  }

  /**
   * Opens video result file and shows video on review result UI.
   * @param {!FileEntry} fileEntry Video result file.
   * @return {!Promise<boolean>} Promise resolved with whether user confirms
   *     with the video result.
   */
  async openVideo(fileEntry) {
    this.reviewVideoResult_.src = fileEntry.toURL();
    cca.state.set('review-video-result', true);
    cca.state.set('review-result', true);
    this.playResultVideoButton_.focus();

    return new Promise((resolve) => {
      this.resolveOpen_ = resolve;
    });
  }
};
