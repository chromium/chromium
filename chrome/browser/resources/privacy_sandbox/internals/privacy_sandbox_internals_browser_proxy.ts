// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {PageHandlerRemote} from './privacy_sandbox_internals.mojom-webui.js';
import {PageHandler} from './privacy_sandbox_internals.mojom-webui.js';

export class PrivacySandboxInternalsBrowserProxy {
  handler: PageHandlerRemote;

  constructor() {
    this.handler = PageHandler.getRemote();
  }

  shouldShowTpcdMetadataGrants(): boolean {
    return loadTimeData.getBoolean('isPrivacySandboxInternalsDevUIEnabled');
  }

  static getInstance(): PrivacySandboxInternalsBrowserProxy {
    return instance || (instance = new PrivacySandboxInternalsBrowserProxy());
  }

  static setInstance(proxy: PrivacySandboxInternalsBrowserProxy) {
    instance = proxy;
  }
}

let instance: PrivacySandboxInternalsBrowserProxy|null = null;
