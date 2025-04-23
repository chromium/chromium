// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NewTabFooterHandlerFactory, NewTabFooterHandlerRemote} from './new_tab_footer.mojom-webui.js';

let instance: NewTabFooterDocumentProxy|null = null;

export class NewTabFooterDocumentProxy {
  static getInstance(): NewTabFooterDocumentProxy {
    if (!instance) {
      const handler = new NewTabFooterHandlerRemote();
      const factoryRemote = NewTabFooterHandlerFactory.getRemote();
      factoryRemote.createPageHandler(handler.$.bindNewPipeAndPassReceiver());
      instance = new NewTabFooterDocumentProxy(handler);
    }
    return instance;
  }

  static setInstance(handler: NewTabFooterHandlerRemote) {
    instance = new NewTabFooterDocumentProxy(handler);
  }

  handler: NewTabFooterHandlerRemote;

  private constructor(handler: NewTabFooterHandlerRemote) {
    this.handler = handler;
  }
}
