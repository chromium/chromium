// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PhotosHandler, PhotosHandlerRemote} from '../../photos.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP Photos module JS to the browser
 * and receiving the browser response.
 */

/** @type {?PhotosHandlerRemote} */
let handler = null;

export class PhotosProxy {
  /** @return {!PhotosHandlerRemote} */
  static getHandler() {
    return handler || (handler = PhotosHandler.getRemote());
  }

  /** @param {!PhotosHandlerRemote} newHandler */
  static setHandler(newHandler) {
    handler = newHandler;
  }

  /** @private */
  constructor() {}
}
