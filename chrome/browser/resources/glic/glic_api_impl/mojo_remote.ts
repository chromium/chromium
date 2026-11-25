// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {maybeWrapWithLogging} from './mojo_logging.js';
import type {MojoLoggingOptions} from './mojo_logging.js';

export interface MojoRemoteLike {
  onConnectionError: {
    addListener(listener: () => void): unknown,
  };
}

export interface WrapMojoRemoteOptions extends MojoLoggingOptions {
  /**
   * When true, method calls on a disconnected remote return a never-settling
   * Promise (and in-flight calls that reject due to pipe closure hang instead
   * of rejecting). Only enable this for pipes scoped to the lifetime of the
   * web client, where disconnection means the client is defunct.
   */
  hangOnDisconnect: boolean;
}

/**
 * Wraps a Mojo Remote with optional debug logging and optional
 * hang-on-disconnect behavior so client-lifetime pipe teardown does not surface
 * spurious unhandled Promise rejections in the guest frame.
 */
export function wrapMojoRemote<T extends MojoRemoteLike>(
    target: T, options: WrapMojoRemoteOptions): T {
  const loggedTarget = maybeWrapWithLogging(target, options);
  if (!options.hangOnDisconnect) {
    return loggedTarget;
  }

  let disconnected = false;
  target.onConnectionError.addListener(() => {
    disconnected = true;
  });

  return new Proxy(loggedTarget, {
    get(targetObj, prop) {
      const origValue = Reflect.get(targetObj, prop, targetObj);
      if (typeof origValue !== 'function' || typeof prop === 'symbol' ||
          prop.startsWith('$')) {
        return origValue;
      }

      return function(...args: unknown[]) {
        if (disconnected) {
          return new Promise(() => {});
        }
        const result = origValue.apply(targetObj, args);
        if (result instanceof Promise) {
          return result.catch((err: unknown) => {
            if (disconnected) {
              return new Promise(() => {});
            }
            throw err;
          });
        }
        return result;
      };
    },
  });
}
