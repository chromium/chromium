// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FeedHandler, FeedHandlerRemote} from '../../feed.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending requests from Feed NTP module JS to the browser
 * and receiving the browser response.
 */

let handler: FeedHandlerRemote|null = null;

export class FeedProxy {
  static getHandler(): FeedHandlerRemote {
    return handler || (handler = FeedHandler.getRemote());
  }

  static setHandler(newHandler: FeedHandlerRemote) {
    handler = newHandler;
  }

  private constructor() {}
}
