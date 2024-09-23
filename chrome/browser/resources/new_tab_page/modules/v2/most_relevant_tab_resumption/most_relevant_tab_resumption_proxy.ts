// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote} from '../../../most_relevant_tab_resumption.mojom-webui.js';
import {PageHandler} from '../../../most_relevant_tab_resumption.mojom-webui.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the page and
 * the browser.
 */

export interface MostRelevantTabResumptionProxy {
  handler: PageHandlerRemote;
}

export class MostRelevantTabResumptionProxyImpl implements
    MostRelevantTabResumptionProxy {
  handler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): MostRelevantTabResumptionProxy {
    if (instance) {
      return instance;
    }

    const handler = PageHandler.getRemote();
    return instance = new MostRelevantTabResumptionProxyImpl(handler);
  }

  static setInstance(obj: MostRelevantTabResumptionProxy) {
    instance = obj;
  }
}

let instance: MostRelevantTabResumptionProxy|null = null;
