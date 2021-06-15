// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter} from './launcher_internals.mojom-webui.js';

export class BrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  // TODO(crbug.com/1211232): Use PageHandlerFactory to create a page handler.

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
