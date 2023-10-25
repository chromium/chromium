// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OnDeviceInternalsPage, OnDeviceInternalsPageRemote} from './on_device_internals_page.mojom-webui.js';

let instance: BrowserProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class BrowserProxy {
  static getInstance(): BrowserProxy {
    if (!instance) {
      instance = new BrowserProxy(OnDeviceInternalsPage.getRemote());
    }
    return instance;
  }

  handler: OnDeviceInternalsPageRemote;

  private constructor(handler: OnDeviceInternalsPageRemote) {
    this.handler = handler;
  }
}
