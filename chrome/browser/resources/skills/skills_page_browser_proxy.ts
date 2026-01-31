// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote, SkillsPageCallbackRouter} from './skills.mojom-webui.js';
import type {PageHandlerInterface} from './skills.mojom-webui.js';

export class SkillsPageBrowserProxy {
  handler: PageHandlerInterface;
  callbackRouter: SkillsPageCallbackRouter;

  private constructor() {
    this.handler = new PageHandlerRemote();
    this.callbackRouter = new SkillsPageCallbackRouter();

    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): SkillsPageBrowserProxy {
    return instance || (instance = new SkillsPageBrowserProxy());
  }

  static setInstance(obj: SkillsPageBrowserProxy) {
    instance = obj;
  }
}

let instance: SkillsPageBrowserProxy|null = null;
