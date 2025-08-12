// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandler} from './autofill_ml_internals.mojom-webui.js';
import type {PageHandlerRemote} from './autofill_ml_internals.mojom-webui.js';

export class AutofillMlInternalsBrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = PageHandler.getRemote();
  }

  static getInstance(): AutofillMlInternalsBrowserProxy {
    return instance || (instance = new AutofillMlInternalsBrowserProxy());
  }
}

let instance: AutofillMlInternalsBrowserProxy|null = null;
