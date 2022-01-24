// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {Resolution} from '../../type.js';

/**
 * @typedef {{
 *   decoderArgs: !Array<string>,
 *   encoderArgs: !Array<string>,
 *   outputExtension: string,
 * }}
 */
export let VideoProcessorArgs;

/**
 * @param {number} videoRotation
 * @param {boolean} outputSeekable
 * @return {!VideoProcessorArgs}
 */
export function createMp4Args(videoRotation, outputSeekable) {
  // input in mkv format
  const decoderArgs = ['-f', 'matroska'];
  const encoderArgs = [
    // rotate the video by metadata
    '-metadata:s:v', `rotate=${videoRotation}`,
    // transcode audio to aac and copy the video
    '-c:a', 'aac', '-c:v', 'copy'  // eslint-disable-line comma-dangle
  ];
  // TODO(crbug.com/1140852): Remove non-seekable code path once the
  // Android camera intent helper support seek operation.
  if (!outputSeekable) {
    // Mark unseekable.
    encoderArgs.push('-seekable', '0');
    // Produce a fragmented MP4.
    encoderArgs.push('-movflags', 'frag_keyframe', '-frag_duration', '100000');
  }
  return {decoderArgs, encoderArgs, outputExtension: 'mp4'};
}


/**
 * @param {!Resolution} resolution
 * @return {!VideoProcessorArgs}
 */
export function createGifArgs({width, height}) {
  // Raw rgba frame input format with fixed resolution.
  const decoderArgs =
      ['-f', 'rawvideo', '-s', `${width}x${height}`, '-pix_fmt', 'rgba'];
  const encoderArgs = [
    // output framerate
    '-r', '15',
    // loop inifinitly
    '-loop', '0'  // eslint-disable-line comma-dangle
  ];
  return {decoderArgs, encoderArgs, outputExtension: 'gif'};
}
