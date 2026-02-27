// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from '../report_unsafe_site.mojom-webui.js';

export interface ReportUnsafeSiteBrowserProxy {
  getPageHandler(): PageHandlerRemote;
}

export class ReportUnsafeSiteBrowserProxyImpl implements
    ReportUnsafeSiteBrowserProxy {
  private handler: PageHandlerRemote;

  private constructor() {
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  getPageHandler(): PageHandlerRemote {
    return this.handler;
  }

  static getInstance(): ReportUnsafeSiteBrowserProxy {
    return instance || (instance = new ReportUnsafeSiteBrowserProxyImpl());
  }

  static setInstance(obj: ReportUnsafeSiteBrowserProxy) {
    instance = obj;
  }
}

let instance: ReportUnsafeSiteBrowserProxy|null = null;
