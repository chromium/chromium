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
 * Used to save captured video.
 * @implements {cca.models.VideoSaver}
 */
cca.models.FileVideoSaver = class {
  /**
   * @param {!FileEntry} file
   * @param {!FileWriter} writer
   * @private
   */
  constructor(file, writer) {
    /**
     * @const {!FileEntry}
     */
    this.file_ = file;

    /**
     * @const {!FileWriter}
     */
    this.writer_ = writer;

    /**
     * Promise of the ongoing write.
     * @type {!Promise}
     */
    this.curWrite_ = Promise.resolve();
  }

  /**
   * @override
   */
  async write(blob) {
    this.curWrite_ = (async () => {
      await this.curWrite_;
      await new Promise((resolve) => {
        this.writer_.onwriteend = resolve;
        this.writer_.write(blob);
      });
    })();
    await this.curWrite_;
  }

  /**
   * @override
   */
  async endWrite() {
    await this.curWrite_;
    return this.file_;
  }

  /**
   * Creates FileVideoSaver.
   * @param {!FileEntry} file The file which FileVideoSaver saves the result
   *     video into.
   * @return {!Promise<!cca.models.FileVideoSaver>}
   */
  static async create(file) {
    const writer = await new Promise(
        (resolve, reject) => file.createWriter(resolve, reject));
    return new cca.models.FileVideoSaver(file, writer);
  }
};
