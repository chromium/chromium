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
 * Used to save captured video into a preview file and forward to intent result.
 * @implements {cca.models.VideoSaver}
 */
cca.models.IntentVideoSaver = class {
  /**
   * @param {!cca.intent.Intent} intent
   * @param {!cca.models.FileVideoSaver} fileSaver
   * @private
   */
  constructor(intent, fileSaver) {
    /**
     * @const {!cca.intent.Intent} intent
     * @private
     */
    this.intent_ = intent;

    /**
     * @const {!cca.models.FileVideoSaver}
     * @private
     */
    this.fileSaver_ = fileSaver;
  }

  /**
   * @override
   */
  async write(blob) {
    await this.fileSaver_.write(blob);
    const arrayBuffer = await blob.arrayBuffer();
    this.intent_.appendData(new Uint8Array(arrayBuffer));
  }

  /**
   * @override
   */
  async endWrite() {
    return this.fileSaver_.endWrite();
  }

  /**
   * Creates IntentVideoSaver.
   * @param {!cca.intent.Intent} intent
   * @return {!Promise<!cca.models.IntentVideoSaver>}
   */
  static async create(intent) {
    const tmpFile = await cca.models.FileSystem.createPrivateTempVideoFile();
    const fileSaver = await cca.models.FileVideoSaver.create(tmpFile);
    return new cca.models.IntentVideoSaver(intent, fileSaver);
  }
};
