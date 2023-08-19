// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Stream constraints for audio and video.
 */
export interface StreamConstraints {
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
 * Converts `constraints` to MediaStreamConstraints that is suitable to be used
 * in getUserMedia.
 */
export function toMediaStreamConstraints(constraints: StreamConstraints):
    MediaStreamConstraints {
  // TODO(pihsun): Investigate why deviceId is '' for fake VCD on non-CrOS
  // environment.
  const videoCostraint = {...constraints.video};
  if (constraints.deviceId !== '') {
    videoCostraint.deviceId = {exact: constraints.deviceId};
  }
  return {
    audio: constraints.audio ? {echoCancellation: false} : false,
    video: videoCostraint,
  };
}
