// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BaseDialogPageHandlerInterface} from './base_dialog.mojom-webui.js';
import {BaseDialogPageHandler} from './base_dialog.mojom-webui.js';

export class BaseDialogBrowserProxy {
  private handler: BaseDialogPageHandlerInterface;

  constructor() {
    this.handler = BaseDialogPageHandler.getRemote();
  }

  static setInstance(proxy: BaseDialogBrowserProxy) {
    instance = proxy;
  }

  static getInstance(): BaseDialogBrowserProxy {
    return instance || (instance = new BaseDialogBrowserProxy());
  }

  getHandler(): BaseDialogPageHandlerInterface {
    return this.handler;
  }
}

let instance: BaseDialogBrowserProxy|null = null;
