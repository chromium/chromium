// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ContextualTasksInternalsPageHandlerRemote} from '../contextual_tasks_internals.mojom-webui.js';
import {ContextualTasksInternalsPageHandler} from '../contextual_tasks_internals.mojom-webui.js';

/**
 * @fileoverview A browser proxy for the ContextualTasks Internals page.
 */
export class BrowserProxy {
  handler: ContextualTasksInternalsPageHandlerRemote;

  constructor() {
    this.handler = ContextualTasksInternalsPageHandler.getRemote();
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }
}

let instance: BrowserProxy|null = null;
