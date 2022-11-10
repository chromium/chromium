// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomizeChromePageHandler, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';

let instance: CustomizeChromeApiProxy|null = null;

export class CustomizeChromeApiProxy {
  handler: CustomizeChromePageHandlerInterface;

  constructor() {
    this.handler = CustomizeChromePageHandler.getRemote();
  }

  static getInstance(): CustomizeChromeApiProxy {
    return instance || (instance = new CustomizeChromeApiProxy());
  }

  static setInstance(newInstance: CustomizeChromeApiProxy) {
    instance = newInstance;
  }
}
