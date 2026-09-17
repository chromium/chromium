// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TokenRange} from './tab_search.mojom-webui.js';
import {SearchHandler} from './tab_search.mojom-webui.js';
import type {SearchHandlerRemote} from './tab_search.mojom-webui.js';

let instance: SearchApiProxy|null = null;

export interface SearchApiProxy {
  getRangesIgnoringCaseAndAccents(searchText: string, targets: string[]):
      Promise<{ranges: TokenRange[][]}>;
}

export class SearchApiProxyImpl implements SearchApiProxy {
  handler: SearchHandlerRemote;

  constructor() {
    this.handler = SearchHandler.getRemote();
  }

  getRangesIgnoringCaseAndAccents(searchText: string, targets: string[]) {
    return this.handler.getRangesIgnoringCaseAndAccents(searchText, targets);
  }

  static getInstance(): SearchApiProxy {
    return instance || (instance = new SearchApiProxyImpl());
  }

  static setInstance(obj: SearchApiProxy) {
    instance = obj;
  }
}
