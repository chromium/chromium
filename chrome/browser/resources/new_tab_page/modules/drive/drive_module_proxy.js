// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './drive.mojom-lite.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

/** @type {DriveProxy} */
let instance = null;

export class DriveProxy {
  /** @return {!DriveProxy} */
  static getInstance() {
    return instance || (instance = new DriveProxy());
  }

  /** @param {DriveProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!drive.mojom.DriveHandlerRemote} */
    this.handler = drive.mojom.DriveHandler.getRemote();
  }
}
