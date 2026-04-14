// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './accessibility_annotator_info.mojom-webui.js';
import {PageHandler} from './accessibility_annotator_info.mojom-webui.js';

export class AccessibilityAnnotatorInfoBrowserProxy {
  handler: PageHandlerInterface;

  constructor() {
    this.handler = PageHandler.getRemote();
  }

  static getInstance(): AccessibilityAnnotatorInfoBrowserProxy {
    return instance ||
        (instance = new AccessibilityAnnotatorInfoBrowserProxy());
  }

  static setInstance(obj: AccessibilityAnnotatorInfoBrowserProxy) {
    instance = obj;
  }
}

let instance: AccessibilityAnnotatorInfoBrowserProxy|null = null;
