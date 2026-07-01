// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebClientHost} from '../request_types.js';
import type {PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

export interface GlicBrowserHostBaseContext {
  readonly router: PostMessageRouter;
  readonly clientRemote: PostMessageRemote<WebClientHost>;
}

// --- Compile-Time Type-Safe Delegation Check Infrastructure ---

type ExcludedKeys = 'initialize'|'constructor';

type CleanMethods<T, N> = {
  [Property in keyof T as Property extends ExcludedKeys ?
       never :
       (Exclude<T[Property], undefined> extends Function ? Property : never)]:
      [N];
};

type MapDelegates<D extends unknown[]> = {
  [I in keyof D]: CleanMethods<D[I], I>;
};

type TupleToIntersection<T extends unknown[]> =
    T extends [infer Head, ...infer Tail] ?
    Pick<Head, keyof Head>&TupleToIntersection<Tail>:
    unknown;

interface Collisions<T> {
  collision:
      {[K in keyof T] -?: [Exclude<T[K], undefined>] extends [never]?
       K: never;}[keyof T];
}

export type CheckCollisions<T, D extends unknown[]> = Collisions<
    TupleToIntersection<[CleanMethods<T, 'target'>, ...MapDelegates<D>]>>[
  'collision'
];

type CheckDelegation<T, D extends unknown[]> =
    CheckCollisions<T, D> extends never ?
    unknown :
    {'Error: Method name collision detected!': CheckCollisions<T, D>};

/**
 * Creates a Proxy that forwards property access to a target object, and falls
 * back to a list of delegate objects if the property is not found on the
 * target. Note that this assumes that once a function is looked up, it is
 * cached permanently.
 *
 * Enforces compile-time disjointness: if any two delegates share a method name,
 * or if a delegate shadows a method implemented on the target, compilation will
 * fail. Also performs runtime checks at creation time to prevent
 * shadowing/conflicts.
 */
export function createDelegationProxy<
    T extends object,
    D extends unknown[],
>(
    target: T,
    delegates: [...D] & CheckDelegation<T, D>,
): Pick<T, keyof T> & TupleToIntersection<D> {
  const registeredMethods = new Set<string>();
  const excludes = new Set(['constructor', 'initialize']);

  const untypedDelegates =
      delegates as unknown as Array<Record<string, unknown>>;

  for (const delegate of untypedDelegates) {
    const proto = Object.getPrototypeOf(delegate);
    if (!proto) {
      continue;
    }
    for (const method of Object.getOwnPropertyNames(proto)) {
      if (excludes.has(method)) {
        continue;
      }
      const val = delegate[method];
      if (typeof val === 'function') {
        const isStandardProperty = Reflect.has(Object.prototype, method);
        if (registeredMethods.has(method) ||
            (!isStandardProperty && Reflect.has(target, method))) {
          throw new Error(
              `Method conflict detected: '${
                  method}' is defined on multiple delegates ` +
              `or is shadowed by a property on the target object.`);
        }
        registeredMethods.add(method);
      }
    }
  }

  const methodCache = new Map<string|symbol, Function>();

  return new Proxy(target, {
           get(t, prop, receiver) {
             const cachedMethod = methodCache.get(prop);
             if (cachedMethod !== undefined) {
               return cachedMethod;
             }

             if (Reflect.has(t, prop)) {
               const val = Reflect.get(t, prop, receiver);
               if (typeof val === 'function') {
                 const bound = val.bind(t);
                 methodCache.set(prop, bound);
                 return bound;
               }
               return val;
             }

             for (const delegate of untypedDelegates) {
               const val = Reflect.get(delegate, prop, delegate);
               if (val !== undefined) {
                 if (typeof val === 'function') {
                   const bound = val.bind(delegate);
                   methodCache.set(prop, bound);
                   return bound;
                 }
                 return val;
               }
             }

             return undefined;
           },
         }) as unknown as Pick<T, keyof T>&
      TupleToIntersection<D>;
}
