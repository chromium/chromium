// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the chrome://search-engine-choice page
 * to interact with the browser.
 */

import {PageHandlerFactory, PageHandlerRemote} from './search_engine_choice.mojom-webui.js';

export interface SearchEngineChoice {
  prepopulate_id: number;
  name: string;
  icon_path: string;
  url: string;
}

export class SearchEngineChoiceBrowserProxy {
  handler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): SearchEngineChoiceBrowserProxy {
    if (instance) {
      return instance;
    }

    const handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        (handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());

    return instance = new SearchEngineChoiceBrowserProxy(handler);
  }

  static setInstance(obj: SearchEngineChoiceBrowserProxy) {
    instance = obj;
  }
}

let instance: SearchEngineChoiceBrowserProxy|null = null;
