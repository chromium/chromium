// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OnDeviceModelService, OnDeviceModelServiceRemote} from './on_device_model.mojom-webui.js';

let instance: BrowserProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class BrowserProxy {
  static getInstance(): BrowserProxy {
    if (!instance) {
      instance = new BrowserProxy(OnDeviceModelService.getRemote());
    }
    return instance;
  }

  handler: OnDeviceModelServiceRemote;

  private constructor(handler: OnDeviceModelServiceRemote) {
    this.handler = handler;
  }
}
