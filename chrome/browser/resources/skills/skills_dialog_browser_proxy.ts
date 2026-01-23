// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './skills.mojom-webui.js';
import {PageHandlerFactory, PageHandlerRemote} from './skills.mojom-webui.js';

/**
 * A browser proxy for the skills dialog. This manages communication between the
 * skills dialog WebUI and the browser process.
 */
export interface SkillsDialogBrowserProxy {
  handler: PageHandlerInterface;
}

export class SkillsDialogBrowserProxyImpl implements SkillsDialogBrowserProxy {
  handler: PageHandlerInterface;

  private constructor() {
    this.handler = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): SkillsDialogBrowserProxy {
    return instance || (instance = new SkillsDialogBrowserProxyImpl());
  }

  static setInstance(obj: SkillsDialogBrowserProxy) {
    instance = obj;
  }
}

let instance: SkillsDialogBrowserProxy|null = null;
