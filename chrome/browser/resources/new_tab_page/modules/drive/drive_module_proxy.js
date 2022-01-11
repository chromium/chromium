// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DriveHandler, DriveHandlerRemote} from '../../drive.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

/** @type {?DriveHandlerRemote} */
let handler = null;

export class DriveProxy {
  /** @return {!DriveHandlerRemote} */
  static getHandler() {
    return handler || (handler = DriveHandler.getRemote());
  }

  /** @param {!DriveHandlerRemote} newHandler */
  static setHandler(newHandler) {
    handler = newHandler;
  }

  /** @private */
  constructor() {}
}
