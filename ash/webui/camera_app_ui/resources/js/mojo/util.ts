// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addUnloadCallback} from '../unload.js';
import {WaitableEvent} from '../waitable_event.js';

const windowUnload = new WaitableEvent();

export interface MojoEndpoint {
  $: {close: () => void};
}

addUnloadCallback(() => {
  windowUnload.signal();
});

const NEVER_SETTLED_PROMISE = new Promise<never>(
    () => {
        // This doesn't call the resolve function so the Promise will never
        // be resolved or rejected.
    });

/**
 * Wraps a mojo response promise so that we can handle the situation when the
 * call is dropped by window unload gracefully.
 *
 * @return Returns the mojo response which will be resolved when getting
 *     response or will never be resolved if the window unload is about to
 *     happen.
 */
async function wrapMojoResponse(call: unknown): Promise<unknown> {
  const result = await Promise.race([windowUnload.wait(), call]);
  if (windowUnload.isSignaled()) {
    return NEVER_SETTLED_PROMISE;
  }
  return result;
}

const mojoResponseHandler: ProxyHandler<MojoEndpoint> = {
  get: function(target, property) {
    const val = Reflect.get(target, property);
    if (val instanceof Function) {
      return (...args: unknown[]) => {
        if (windowUnload.isSignaled()) {
          // Don't try to call the mojo function if window is already unloaded,
          // since the connection would have already been closed, and there
          // would be uncaught exception if we try to call the mojo function.
          return NEVER_SETTLED_PROMISE;
        }
        return wrapMojoResponse(Reflect.apply(val, target, args));
      };
    }
    return val;
  },
};

/**
 * Closes the given mojo endpoint once the page is unloaded.
 * Reference b/176139064.
 */
function closeWhenUnload(endpoint: MojoEndpoint) {
  addUnloadCallback(() => closeEndpoint(endpoint));
}

/**
 * Returns a proxy of |endpoint|.
 *
 * The |endpoint| is automatically closed on window unload.
 * Note that the methods on the returned proxy will not be resolved after unload
 * event on window is triggered to avoid race condition during the window
 * unloading.
 */
export function wrapEndpoint<T extends MojoEndpoint>(endpoint: T): T {
  closeWhenUnload(endpoint);
  // The mojoResponseHandler is designed to be able to handle all mojo
  // connection proxies.
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return new Proxy(endpoint, mojoResponseHandler as ProxyHandler<T>);
}

/**
 * Closes the target mojo |endpoint|.
 */
export function closeEndpoint(endpoint: MojoEndpoint): void {
  endpoint.$.close();
}

/**
 * Returns a fake endpoint using proxy.
 */
export function fakeEndpoint<T>(): T {
  // Disable type assertion since it is intended to make all function calls as
  // no-ops.
  const handler = {
    apply: (): unknown =>
        new Proxy(() => {/* Doing nothing for fake */}, handler),
    get: (): unknown =>
        new Proxy(() => {/* Doing nothing for fake */}, handler),
  };
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return new Proxy({}, handler) as T;
}
