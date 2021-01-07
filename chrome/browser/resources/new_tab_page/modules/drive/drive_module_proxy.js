// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './drive.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from NTP dummy module JS to the browser
 * and receiving the browser response.
 */

export class DriveProxy {
  constructor() {
    /** @type {!drive.mojom.DriveHandlerRemote} */
    this.handler = drive.mojom.DriveHandler.getRemote();
  }
}

addSingletonGetter(DriveProxy);
