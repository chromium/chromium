// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Comlink from './lib/comlink.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * @type {!WaitableEvent}
 */
const domReady = new WaitableEvent();

const exposedObjects = {loadScript};

/**
 * Loads given script into the untrusted context.
 * @param {string} scriptUrl
 * @return {!Promise}
 */
async function loadScript(scriptUrl) {
  await domReady.wait();
  const module = await import(scriptUrl);
  Object.assign(exposedObjects, module);
}

document.addEventListener('DOMContentLoaded', () => {
  domReady.signal();
});

Comlink.expose(exposedObjects, Comlink.windowEndpoint(self.parent, self));
