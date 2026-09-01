// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getGuestLoadTimeData} from './guest_load_time_data.js';

const MAX_ARRAY_LENGTH = 10;

/**
 * Converts a value to a JSON string for debug logging.
 *
 * Inclusively serializes primitive types, BigInt, plain objects, Arrays, Maps,
 * Sets, and ArrayBuffers, while preventing traversal into arbitrary class
 * instances and circular structures. Long arrays are truncated with '...'.
 */
export function toDebugJson(v: unknown): string {
  try {
    const seen = new WeakSet();
    return JSON.stringify(v, (_key, value) => {
      if (typeof value === 'bigint') {
        return value.toString();
      }
      if (typeof value !== 'object' || value === null) {
        return value;
      }
      if (seen.has(value)) {
        return '[Circular]';
      }
      seen.add(value);

      if (Array.isArray(value)) {
        if (value.length > MAX_ARRAY_LENGTH) {
          return [...value.slice(0, MAX_ARRAY_LENGTH), '...'];
        }
        return value;
      }
      if (value instanceof Set) {
        const arr = Array.from(value);
        if (arr.length > MAX_ARRAY_LENGTH) {
          return [...arr.slice(0, MAX_ARRAY_LENGTH), '...'];
        }
        return arr;
      }
      if (value instanceof Map) {
        try {
          return Object.fromEntries(value.entries());
        } catch {
          return Array.from(value.entries());
        }
      }
      if (value instanceof ArrayBuffer) {
        return `ArrayBuffer(${value.byteLength})`;
      }
      if (ArrayBuffer.isView(value)) {
        return `${value.constructor.name}(${value.byteLength})`;
      }
      if (value instanceof Error) {
        return `${value.name}: ${value.message}`;
      }
      if (value instanceof Date || value instanceof RegExp) {
        return value;
      }
      const proto = Object.getPrototypeOf(value);
      if (proto === Object.prototype || proto === null) {
        return value;
      }
      // For any other object or class instance, avoid traversing internal
      // state.
      return `[${value.constructor?.name || 'Object'}]`;
    });
  } catch (err) {
    return String(v);
  }
}

export interface MojoLoggingOptions {
  enabled?: boolean;
  prefix?: string;
  ignoreMethods?: Set<string>;
}

/**
 * Wraps a Mojo Remote or Receiver handler object in a logging Proxy if logging
 * is enabled (either via `options.enabled` or
 * `getGuestLoadTimeData().loggingEnabled`). If logging is disabled, returns
 * `target` directly with no wrapper or performance overhead.
 */
export function maybeWrapWithLogging<T extends object>(
    target: T,
    options: MojoLoggingOptions = {},
    ): T {
  const enabled =
      options.enabled ?? (getGuestLoadTimeData().loggingEnabled ?? false);
  if (!enabled) {
    return target;
  }

  const ignoreMethods = options.ignoreMethods ?? new Set(['checkResponsive']);
  const prefix = options.prefix ?? target.constructor?.name ?? 'Mojo';

  return new Proxy(target, {
    get(targetObj, prop, receiver) {
      const origValue = Reflect.get(targetObj, prop, receiver);

      // Do not intercept Mojo internals (e.g. `remote.$`) or non-functions.
      if (typeof origValue !== 'function' || typeof prop === 'symbol' ||
          prop.startsWith('$')) {
        return origValue;
      }

      const methodName = String(prop);
      if (ignoreMethods.has(methodName)) {
        return origValue.bind(targetObj);
      }

      return function(...args: unknown[]) {
        console.info(
            `${prefix} [${methodName}] sending request: ${toDebugJson(args)}`,
            args,
        );

        try {
          const result = origValue.apply(targetObj, args);

          if (result instanceof Promise) {
            return result.then(
                (response) => {
                  console.info(
                      `${prefix} [${methodName}] received response: ${
                          toDebugJson(response)}`,
                      response,
                  );
                  return response;
                },
                (error) => {
                  console.warn(
                      `${prefix} [${methodName}] received error:`,
                      error,
                  );
                  throw error;
                },
            );
          }

          return result;
        } catch (error) {
          console.warn(
              `${prefix} [${methodName}] threw error synchronously:`,
              error,
          );
          throw error;
        }
      };
    },
  });
}
