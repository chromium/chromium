// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class MediaDevicesProxy {
  /** @return {!MediaDevices} */
  static getMediaDevices() {
    return mediaDevicesInstance || navigator.mediaDevices;
  }

  /** @param {!MediaDevices} obj */
  static setMediaDevicesForTesting(obj) {
    mediaDevicesInstance = obj;
  }
}

/** @type {?MediaDevices} */
let mediaDevicesInstance = null;