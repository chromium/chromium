// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote} from './privacy_sandbox_internals.mojom-webui.js';
import {PageHandler} from './privacy_sandbox_internals.mojom-webui.js';

export class PrivacySandboxInternalsBrowserProxy {
  handler: PageHandlerRemote;

  constructor() {
    this.handler = PageHandler.getRemote();
  }

  static getInstance(): PrivacySandboxInternalsBrowserProxy {
    return instance || (instance = new PrivacySandboxInternalsBrowserProxy());
  }

  static setInstance(proxy: PrivacySandboxInternalsBrowserProxy) {
    instance = proxy;
  }
}

let instance: PrivacySandboxInternalsBrowserProxy|null = null;
