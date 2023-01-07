// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Comlink from './lib/comlink.js';
import {WaitableEvent} from './waitable_event.js';

const domReady = new WaitableEvent();

const exposedObjects = {loadScript};

/**
 * Loads given script into the untrusted context.
 */
async function loadScript(scriptUrl: string): Promise<void> {
  await domReady.wait();
  const module = await import(scriptUrl);
  Object.assign(exposedObjects, module);
}

document.addEventListener('DOMContentLoaded', () => {
  domReady.signal();
});

Comlink.expose(exposedObjects, Comlink.windowEndpoint(self.parent, self));
