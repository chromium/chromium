// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandler, PageHandlerRemote} from '../../../tab_resumption.mojom-webui.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the page and
 * the browser.
 */

export interface TabResumptionProxy {
  handler: PageHandlerRemote;
}

export class TabResumptionProxyImpl implements TabResumptionProxy {
  handler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): TabResumptionProxy {
    if (instance) {
      return instance;
    }

    const handler = PageHandler.getRemote();
    return instance = new TabResumptionProxyImpl(handler);
  }

  static setInstance(obj: TabResumptionProxy) {
    instance = obj;
  }
}

let instance: TabResumptionProxy|null = null;
