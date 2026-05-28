// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './personal_context_notice.mojom-webui.js';
import {PageHandler} from './personal_context_notice.mojom-webui.js';

export class PersonalContextNoticeBrowserProxy {
  handler: PageHandlerInterface;

  constructor() {
    this.handler = PageHandler.getRemote();
  }

  static getInstance(): PersonalContextNoticeBrowserProxy {
    return instance || (instance = new PersonalContextNoticeBrowserProxy());
  }

  static setInstance(obj: PersonalContextNoticeBrowserProxy) {
    instance = obj;
  }
}

let instance: PersonalContextNoticeBrowserProxy|null = null;
