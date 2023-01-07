// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stream constraints for audio and video.
 */
export interface StreamConstraints {
  /**
   * Target device id.
   */
  deviceId: string;

  /**
   * Extra video constraints.
   */
  video: MediaTrackConstraints;

  /**
   * Whether to enable audio.
   */
  audio: boolean;
}

/**
 * Convert this to MediaStreamConstraints that is suitable to be used in
 * getUserMedia.
 */
export function toMediaStreamConstraints(constraints: StreamConstraints):
    MediaStreamConstraints {
  return {
    audio: constraints.audio ? {echoCancellation: false} : false,
    video: {...constraints.video, deviceId: {exact: constraints.deviceId}},
  };
}
