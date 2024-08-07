// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PrivateStateTokensPageHandlerInterface} from './private_state_tokens.mojom-webui.js';
import {PrivateStateTokensPageHandler} from './private_state_tokens.mojom-webui.js';

export interface PrivateStateTokensApiBrowserProxy {
  handler: PrivateStateTokensPageHandlerInterface;
}

export class PrivateStateTokensApiBrowserProxyImpl implements
    PrivateStateTokensApiBrowserProxy {
  handler: PrivateStateTokensPageHandlerInterface =
      PrivateStateTokensPageHandler.getRemote();

  private constructor() {}

  static getInstance(): PrivateStateTokensApiBrowserProxy {
    return instance || (instance = new PrivateStateTokensApiBrowserProxyImpl());
  }

  static setInstance(proxy: PrivateStateTokensApiBrowserProxy) {
    instance = proxy;
  }
}

let instance: PrivateStateTokensApiBrowserProxy|null = null;
