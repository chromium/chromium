// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DriveHandler, DriveHandlerRemote} from '../../drive.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

let handler: DriveHandlerRemote|null = null;

export class DriveProxy {
  static getHandler(): DriveHandlerRemote {
    return handler || (handler = DriveHandler.getRemote());
  }

  static setHandler(newHandler: DriveHandlerRemote) {
    handler = newHandler;
  }

  private constructor() {}
}
