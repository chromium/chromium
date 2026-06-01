// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PasswordManagerPageHandlerInterface} from '../password_manager.mojom-webui.js';
import {PasswordManagerPageHandlerFactory, PasswordManagerPageHandlerRemote} from '../password_manager.mojom-webui.js';

export interface PasswordManagerBrowserProxy {
  handler: PasswordManagerPageHandlerInterface;
}

export class PasswordManagerBrowserProxyImpl implements
    PasswordManagerBrowserProxy {
  handler: PasswordManagerPageHandlerInterface;

  private constructor() {
    this.handler = new PasswordManagerPageHandlerRemote();
    PasswordManagerPageHandlerFactory.getRemote()
        .createPasswordManagerPageHandler(
            (this.handler as PasswordManagerPageHandlerRemote)
                .$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): PasswordManagerBrowserProxy {
    return instance || (instance = new PasswordManagerBrowserProxyImpl());
  }

  static setInstance(proxy: PasswordManagerBrowserProxy) {
    instance = proxy;
  }
}

let instance: PasswordManagerBrowserProxy|null = null;
