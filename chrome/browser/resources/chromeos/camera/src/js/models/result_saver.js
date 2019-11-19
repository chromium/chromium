// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for models.
 */
cca.models = cca.models || {};

/**
 * Handles captured result photos and video.
 * @interface
 */
cca.models.ResultSaver = class {
  /**
   * Saves photo capture result.
   * @param {!Blob} blob Data of the photo to be added.
   * @param {string} name Name of the photo to be saved.
   * @return {!Promise} Promise for the operation.
   */
  async savePhoto(blob, name) {}

  /**
   * Returns a video saver to save captured result video.
   * @return {!Promise<!cca.models.VideoSaver>}
   */
  async startSaveVideo() {}

  /**
   * Saves captured video result.
   * @param {!cca.models.VideoSaver} video Contains the video result to be
   *     saved.
   * @param {string} name Name of the video to be saved.
   * @return {!Promise}
   */
  async finishSaveVideo(video, name) {}
};
