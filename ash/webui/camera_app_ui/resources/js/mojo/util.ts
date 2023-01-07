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
async function wrapMojoResponse<T>(call: Promise<T>|undefined): Promise<T> {
  const result = await Promise.race([windowUnload.wait(), call]);
  if (windowUnload.isSignaled()) {
    return NEVER_SETTLED_PROMISE;
  }
  return result as T;
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
 *
 * @param endpoint The mojo endpoint.
 */
function closeWhenUnload(endpoint: MojoEndpoint) {
  addUnloadCallback(() => closeEndpoint(endpoint));
}

/**
 * Returns a mojo |endpoint| and returns a proxy of it.
 *
 * @return The proxy of the given endpoint.
 */
export function wrapEndpoint<T extends MojoEndpoint>(endpoint: T): T {
  closeWhenUnload(endpoint);
  return new Proxy(endpoint, mojoResponseHandler as ProxyHandler<T>);
}

/**
 * Returns the target mojo endpoint.
 */
export function closeEndpoint(endpoint: MojoEndpoint): void {
  endpoint.$.close();
}
