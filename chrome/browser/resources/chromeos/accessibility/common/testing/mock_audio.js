// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** A mock audio API for tests. */
class MockAudio {
  constructor() {
    /** @private {boolean} */
    this.muted_ = false;

    /** @private {!Object<string, string>} */
    this.StreamType = {
      INPUT: 'INPUT',
      OUTPUT: 'OUTPUT',
    };
  }

  /**
   * @param {!chrome.audio.StreamType} streamType
   * @param {boolean} isMuted
   * @param {?function} callback
   */
  setMute(streamType, isMuted, callback) {
    this.muted_ = isMuted;
    callback();
  }

  /**
   * @param {!chrome.audio.StreamType} streamType
   * @param {?function(boolean)} callback
   */
  getMute(streamType, callback) {
    callback(this.muted_);
  }
}
