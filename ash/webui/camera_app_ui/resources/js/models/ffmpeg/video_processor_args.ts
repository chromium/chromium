// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Resolution} from '../../type.js';

export interface VideoProcessorArgs {
  decoderArgs: string[];
  encoderArgs: string[];
  outputExtension: string;
}

/**
 * Creates the command line arguments to ffmpeg for mp4 recording.
 */
export function createMp4Args(
    videoRotation: number, outputSeekable: boolean): VideoProcessorArgs {
  // input in mkv format
  const decoderArgs = ['-f', 'matroska'];

  // clang-format formats one argument per line, which makes the list harder
  // to read with comments.
  // clang-format off
  const encoderArgs = [
    // rotate the video by metadata
    '-metadata:s:v', `rotate=${videoRotation}`,
    // transcode audio to aac and copy the video
    '-c:a', 'aac', '-c:v', 'copy',
  ];
  // clang-format on

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
 * Creates the command line arguments to ffmpeg for gif recording.
 */
export function createGifArgs({width, height}: Resolution): VideoProcessorArgs {
  // Raw rgba frame input format with fixed resolution.
  const decoderArgs =
      ['-f', 'rawvideo', '-s', `${width}x${height}`, '-pix_fmt', 'rgba'];

  // clang-format formats one argument per line, which makes the list harder
  // to read with comments.
  // clang-format off
  const encoderArgs = [
    // output framerate
    '-r', '15',
    // loop inifinitly
    '-loop', '0',
  ];
  // clang-format on

  return {decoderArgs, encoderArgs, outputExtension: 'gif'};
}

/**
 * Creates the command line arguments to ffmpeg for time-lapse recording.
 */
export function createTimeLapseArgs(
    {width, height}: Resolution, fps: number,
    videoRotation = 0): VideoProcessorArgs {
  // clang-format off
  const decoderArgs = [
    // input format
    '-f', 'h264',
    // force input framerate
    '-r', `${fps}`,
    // specify video size
    '-s', `${width}x${height}`,
  ];

  // clang-format formats one argument per line, which makes the list harder
  // to read with comments.
  // clang-format off
  const encoderArgs = [
    // rotate the video by metadata
    '-metadata:s:v', `rotate=${videoRotation}`,
    // disable audio and copy the video stream
    '-an', '-c:v', 'copy',
  ];
  // clang-format on

  return {decoderArgs, encoderArgs, outputExtension: 'mp4'};
}
