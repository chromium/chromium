// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './photos.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP Photos module JS to the browser
 * and receiving the browser response.
 */

/** @type {PhotosProxy} */
let instance = null;

export class PhotosProxy {
  /** @return {!PhotosProxy} */
  static getInstance() {
    return instance || (instance = new PhotosProxy());
  }

  /** @param {PhotosProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!photos.mojom.PhotosHandlerRemote} */
    this.handler = photos.mojom.PhotosHandler.getRemote();
  }
}
