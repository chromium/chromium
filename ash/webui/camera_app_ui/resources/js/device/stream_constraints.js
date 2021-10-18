// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stream constraints for audio and video.
 * @record
 */
export class StreamConstraints {
  /**
   * @public
   */
  constructor() {
    /**
     * @type {string} Target device id.
     */
    this.deviceId;

    /**
     * @type {!MediaTrackConstraints} Extra video constraints.
     */
    this.video;

    /**
     * @type {boolean} Whether to enable audio.
     */
    this.audio;
  }
}

/**
 * Convert this to MediaStreamConstraints that is suitable to be used in
 * getUserMedia.
 *
 * @param {!StreamConstraints} constraints
 * @return {!MediaStreamConstraints}
 */
export function toMediaStreamConstraints(constraints) {
  return {
    audio: constraints.audio ? {echoCancellation: false} : false,
    video: {...constraints.video, deviceId: {exact: constraints.deviceId}},
  };
}
