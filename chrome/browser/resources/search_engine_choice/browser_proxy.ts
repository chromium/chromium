// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the chrome://search-engine-choice page
 * to interact with the browser.
 */

import {PageHandlerFactory, PageHandlerInterface, PageHandlerRemote} from './search_engine_choice.mojom-webui.js';

export interface SearchEngineChoice {
  name: string;
}

export class SearchEngineChoiceBrowserProxy {
  handler: PageHandlerInterface;

  constructor() {
    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): SearchEngineChoiceBrowserProxy {
    return instance || (instance = new SearchEngineChoiceBrowserProxy());
  }

  static setInstance(obj: SearchEngineChoiceBrowserProxy) {
    instance = obj;
  }
}

let instance: SearchEngineChoiceBrowserProxy|null = null;
