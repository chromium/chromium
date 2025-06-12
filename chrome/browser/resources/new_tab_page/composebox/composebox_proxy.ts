// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ComposeboxPageHandlerRemote} from '../composebox.mojom-webui.js';
import {ComposeboxPageHandler} from '../composebox.mojom-webui.js';

export interface ComposeboxProxy {
  handler: ComposeboxPageHandlerRemote;
}

export class ComposeboxProxyImpl implements ComposeboxProxy {
  handler: ComposeboxPageHandlerRemote;

  constructor(handler: ComposeboxPageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): ComposeboxProxyImpl {
    return instance ||
        (instance = new ComposeboxProxyImpl(ComposeboxPageHandler.getRemote()));
  }

  static setInstance(newInstance: ComposeboxProxy) {
    instance = newInstance;
  }
}

let instance: ComposeboxProxy|null = null;
