// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DialogHandlerInterface} from './skills.mojom-webui.js';
import {DialogHandlerRemote, PageHandlerFactory} from './skills.mojom-webui.js';

export class SkillsDialogBrowserProxy {
  handler: DialogHandlerInterface;

  private constructor() {
    this.handler = new DialogHandlerRemote();

    PageHandlerFactory.getRemote().createDialogHandler(
        (this.handler as DialogHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): SkillsDialogBrowserProxy {
    return instance || (instance = new SkillsDialogBrowserProxy());
  }

  static setInstance(obj: SkillsDialogBrowserProxy) {
    instance = obj;
  }
}

let instance: SkillsDialogBrowserProxy|null = null;
