// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addUnloadCallback} from '../unload.js';
import {WaitableEvent} from '../waitable_event.js';

const windowUnload = new WaitableEvent();

/**
 * @typedef {{$: {close: function(): void}}}
 */
export let MojoEndpoint;


addUnloadCallback(() => {
  windowUnload.signal();
});

/**
 * Wraps a mojo response promise so that we can handle the situation when the
 * call is dropped by window unload gracefully.
 * @param {!Promise} call
 * @return {!Promise} Returns the mojo response which will be resolved when
 *     getting response or will never be resolved if the window unload is about
 *     to happen.
 */
async function wrapMojoResponse(call) {
  const result = await Promise.race([windowUnload.wait(), call]);
  if (windowUnload.isSignaled()) {
    return new Promise(() => {});
  }
  return result;
}

const mojoResponseHandler = {
  get: function(target, property) {
    if (target[property] instanceof Function) {
      return (...args) => wrapMojoResponse(target[property](...args));
    }
    return target[property];
  },
};

/**
 * Closes the given mojo endpoint once the page is unloaded.
 * Reference b/176139064.
 * @param {!MojoEndpoint} endpoint The mojo endpoint.
 */
function closeWhenUnload(endpoint) {
  addUnloadCallback(() => closeEndpoint(endpoint));
}

/**
 * Returns a mojo |endpoint| and returns a proxy of it.
 * @param {!T} endpoint
 * @return {!T} The proxy of the given endoiubt.
 * @template T
 */
export function wrapEndpoint(endpoint) {
  closeWhenUnload(endpoint);
  return /** @type {!T} */ (new Proxy(endpoint, mojoResponseHandler));
}

/**
 * Returns the target mojo endpoint.
 * @param {!MojoEndpoint} endpoint
 */
export function closeEndpoint(endpoint) {
  endpoint.$.close();
}
