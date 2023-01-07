// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as webcamUtils from 'chrome://resources/ash/common/cr_picture/webcam_utils.js';

/**
 * @fileoverview methods to make it easier to mock out functions for testing.
 */

let webcamUtilsInstance = webcamUtils;

/** Set a mock value for testing. */
export function setWebcamUtilsForTesting(replacement: typeof webcamUtils) {
  webcamUtilsInstance = replacement;
}

export function getWebcamUtils(): typeof webcamUtils {
  return webcamUtilsInstance;
}

let instance: GetUserMediaProxy|null = null;

/** Wrapper around browser media API to mock out for tests. */
export class GetUserMediaProxy {
  static setInstanceForTesting(replacement: GetUserMediaProxy) {
    instance = replacement;
  }

  static getInstance(): GetUserMediaProxy {
    return instance || (instance = new GetUserMediaProxy());
  }

  getUserMedia(): Promise<MediaStream> {
    return navigator.mediaDevices.getUserMedia({
      audio: false,
      video: getWebcamUtils().kDefaultVideoConstraints,
    });
  }
}
