// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './office_fallback.mojom-webui.js';

export abstract class OfficeFallbackBrowserProxy {
  handler: PageHandlerRemote;

  constructor() {
    this.handler = new PageHandlerRemote();
  }

  static getInstance(): OfficeFallbackBrowserProxy {
    return instance || (instance = new OfficeFallbackBrowserProxyImpl());
  }

  static setInstance(obj: OfficeFallbackBrowserProxy) {
    instance = obj;
  }

  abstract getDialogArguments(): string;
}

class OfficeFallbackBrowserProxyImpl extends OfficeFallbackBrowserProxy {
  constructor() {
    super();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  // JSON-encoded dialog arguments.
  getDialogArguments(): string {
    return chrome.getVariableValue('dialogArguments');
  }
}

let instance: OfficeFallbackBrowserProxy|null = null;
