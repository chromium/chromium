// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './skills.mojom-webui.js';
import type {PageHandlerInterface} from './skills.mojom-webui.js';

export class SkillsPageBrowserProxy {
  handler: PageHandlerInterface;

  private constructor() {
    this.handler = new PageHandlerRemote();

    PageHandlerFactory.getRemote().createPageHandler(
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
