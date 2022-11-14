// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomizeChromePageHandler, CustomizeChromePageHandlerInterface} from './customize_chrome.mojom-webui.js';

let instance: CustomizeChromeApiProxy|null = null;

export class CustomizeChromeApiProxy {
  static getInstance(): CustomizeChromeApiProxy {
    if (!instance) {
      const handler = CustomizeChromePageHandler.getRemote();
      instance = new CustomizeChromeApiProxy(handler);
    }
    return instance;
  }

  static setInstance(handler: CustomizeChromePageHandlerInterface) {
    instance = new CustomizeChromeApiProxy(handler);
  }

  handler: CustomizeChromePageHandlerInterface;

  private constructor(handler: CustomizeChromePageHandlerInterface) {
    this.handler = handler;
  }
}
