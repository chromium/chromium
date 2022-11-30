// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {convertImageSequenceToPng, convertImageSequenceToPngBinary} from './png.js';

/**
 * Dimensions for camera capture.
 */
export const CAPTURE_SIZE = {
  height: 576,
  width: 576,
};

/**
 * Interval between frames for camera capture (milliseconds).
 * @const
 */
export const CAPTURE_INTERVAL_MS = 1000 / 10;

/**
 * Duration of camera capture (milliseconds).
 * @type {number}
 */
export const CAPTURE_DURATION_MS = 1000;

/**
 * Default media constraints to request the front facing webcam.
 */
export const kDefaultVideoConstraints = {
  facingMode: 'user',
  height: {ideal: CAPTURE_SIZE.height},
  width: {ideal: CAPTURE_SIZE.width},
};

/**
 * Stops all video tracks associated with a MediaStream object.
 * @param {?MediaStream} stream
 */
export function stopMediaTracks(stream) {
  if (stream) {
    stream.getVideoTracks().forEach(track => track.stop());
  }
}


/**
 * Allocates a canvas for capturing a single still frame at a specific size.
 * @param {{width: number, height: number}} size Frame size.
 * @return {!HTMLCanvasElement} The allocated canvas.
 */
function allocateFrame(size) {
  const canvas =
      /** @type {!HTMLCanvasElement} */ (document.createElement('canvas'));
  canvas.width = size.width;
  canvas.height = size.height;
  const ctx = /** @type {!CanvasRenderingContext2D} */ (
      canvas.getContext('2d', {alpha: false}));
  // Flip frame horizontally.
  ctx.translate(size.width, 0);
  ctx.scale(-1.0, 1.0);
  return canvas;
}

/**
 * Captures a single still frame from a <video> element, placing it at the
 * current drawing origin of a canvas context.
 * @param {!HTMLVideoElement} video Video element to capture from.
 * @param {!HTMLCanvasElement} canvas Canvas to save frame in.
 * @return {!HTMLCanvasElement} The canvas frame was saved in.
 */
function writeVideoFrameToCanvas(video, canvas) {
  const ctx =
      /** @type {!CanvasRenderingContext2D} */ (
          canvas.getContext('2d', {alpha: false}));
  const width = video.videoWidth;
  const height = video.videoHeight;
  if (width < canvas.width || height < canvas.height) {
    console.error(
        'Video capture size too small: ' + width + 'x' + height + '!');
  }
  const src = {};
  if (width / canvas.width > height / canvas.height) {
    // Full height, crop left/right.
    src.height = height;
    src.width = height * canvas.width / canvas.height;
  } else {
    // Full width, crop top/bottom.
    src.width = width;
    src.height = width * canvas.height / canvas.width;
  }
  src.x = (width - src.width) / 2;
  src.y = (height - src.height) / 2;
  ctx.drawImage(
      video, src.x, src.y, src.width, src.height, 0, 0, canvas.width,
      canvas.height);
  return canvas;
}

/**
 * Captures one frame from |video| every |intervalMs|, [numFrames] times.
 * @param {!HTMLVideoElement} video
 * @param {{height: number, width: number}} captureSize
 * @param {number} intervalMs
 * @param {number} numFrames
 * @return {!Promise<!Array<!HTMLCanvasElement>>} an array of canvas elements of
 *     the captured frames
 */
export async function captureFrames(video, captureSize, intervalMs, numFrames) {
  return new Promise((resolve) => {
    if (numFrames <= 0) {
      throw new Error('numFrames must be greater than 0');
    }
    if (intervalMs <= 0) {
      throw new Error('intervalMs must be greater than 0');
    }
    /** Pre-allocate all frames needed for capture. */
    const frames = [];
    while (frames.length < numFrames) {
      frames.push(allocateFrame(captureSize));
    }

    const capturedFrames = [];

    const interval = window.setInterval(() => {
      /** Stop capturing frames when all allocated frames have been consumed. */
      if (frames.length) {
        capturedFrames.push(writeVideoFrameToCanvas(video, frames.pop()));
      } else {
        window.clearInterval(interval);
        resolve(capturedFrames);
      }
    }, intervalMs);
  });
}

/**
 * Mirrors the array around the last element by appending a reversed copy of
 * itself (minus the first and last element). This makes playback appear
 * continuous when played in a loop.
 * @example ['a', 'b', 'c'] => ['a', 'b', 'c', 'b']
 * @param {!Array<T>} arr
 * @return {!Array<T>}
 * @template T
 */
function mirror(arr) {
  return arr.concat(arr.slice(1, -1).reverse());
}

/**
 * Encode frames and convert to animated PNG image as a data URL.
 * @param {!Array<!HTMLCanvasElement>} frames The frames to convert to image.
 * @return {!string} The data URL for image.
 */
export function convertFramesToPng(frames) {
  /** Encode captured frames. */
  const encodedImages = frames.map(function(frame) {
    return frame.toDataURL('image/png');
  });

  /** No need for further processing if single frame. */
  if (encodedImages.length === 1) {
    return encodedImages[0];
  }

  /** Create forward/backward image sequence. */
  const forwardBackwardImageSequence = mirror(encodedImages);

  /** Convert image sequence to animated PNG. */
  return convertImageSequenceToPng(forwardBackwardImageSequence);
}

/**
 * Encode frames and convert to animated PNG image as a Uint8Array.
 * @param {!Array<!HTMLCanvasElement>} frames
 * @return {Uint8Array}
 */
export function convertFramesToPngBinary(frames) {
  const encodedImages = frames.map(frame => frame.toDataURL('image/png'));
  return convertImageSequenceToPngBinary(mirror(encodedImages));
}
