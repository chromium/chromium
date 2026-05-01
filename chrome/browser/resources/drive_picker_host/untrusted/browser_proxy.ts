// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DrivePickerUntrustedHostHandler} from './drive_picker_host_untrusted.mojom-webui.js';
import type {DrivePickerUntrustedHostHandlerRemote} from './drive_picker_host_untrusted.mojom-webui.js';

export interface BrowserProxy {
  handler: DrivePickerUntrustedHostHandlerRemote;
}

export class BrowserProxyImpl implements BrowserProxy {
  handler: DrivePickerUntrustedHostHandlerRemote;

  constructor() {
    this.handler = DrivePickerUntrustedHostHandler.getRemote();
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxyImpl());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
