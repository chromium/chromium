// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple mutation utility to work with immutable state in a
 * more convenient way. The API is intentionally designed to work like a minimal
 * version of https://github.com/immerjs/immer.
 */

import {assert} from './assert.js';

// A special tag to retrieve the non-proxied object by finalizing drafts.
const tag = Symbol('draft');

// A special tag to annotate that a property is deleted in the shadow map.
const deleted = Symbol('deleted');

export type Draft<T> = T&{[tag]: T};

// TODO(shik): This is currently too broad. We need something like
// isPlainObject().
function isDraftable(x: unknown): x is object {
  return typeof x === 'object' && x !== null;
}

function isDraft<T>(x: Draft<T>|T): x is Draft<T> {
  return isDraftable(x) && Reflect.has(x, tag);
}

// TODO(shik): Support ES6 Map/Set.
// TODO(shik): Support custom class.
function createDraft<T>(base: T): Draft<T> {
  // TODO(pihsun): Support other types after allowing recipe to return new
  // value.
  assert(isDraftable(base));
  // A shadow version of the accessed properties will be stored here.
  // A corresponding proxied draft will also be created if it's draftable.
  const shadowMap = new Map<string|symbol, unknown>();
  // Need to force `base` type to `Draft<T>` since [tag] is handled in the
  // proxy handler.
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return new Proxy(base as Draft<T>, {
    get(target, prop, receiver) {
      if (prop === tag) {
        if (shadowMap.size === 0) {
          return base;
        }
        // TODO(shik): Optimize the performance by checking whether object is
        // really mutated. Note that some shadowMap entries might just created
        // for read only.
        const copy = Array.isArray(base) ? [...base] : {...base};
        for (const [key, value] of shadowMap.entries()) {
          if (value === deleted) {
            Reflect.deleteProperty(copy, key);
          } else {
            Reflect.set(
              copy,
              key,
              isDraft(value) ? finalizeDraft(value) : value,
            );
          }
        }
        return copy;
      }

      if (shadowMap.has(prop)) {
        const shadow = shadowMap.get(prop);
        return shadow === deleted ? undefined : shadow;
      }
      const value = Reflect.get(target, prop, receiver);
      if (!isDraftable(value)) {
        return value;
      }
      const shadow = createDraft(value);
      shadowMap.set(prop, shadow);
      return shadow;
    },
    set(_target, prop, newValue) {
      shadowMap.set(
        prop,
        isDraftable(newValue) ? createDraft(newValue) : newValue,
      );
      return true;
    },
    has(target, prop) {
      return prop === tag || Reflect.has(target, prop);
    },
    deleteProperty(target, prop) {
      const found = shadowMap.has(prop) ? shadowMap.get(prop) !== deleted :
                                          Reflect.has(target, prop);
      if (found) {
        shadowMap.set(prop, deleted);
      }
      return found;
    },
  });
}

function finalizeDraft<T>(draft: Draft<T>): T {
  return draft[tag];
}

/**
 * Produces a new object by applying all mutations in the recipe, but keep the
 * original `base` object intact.
 * TODO(shik): Support a special `nothing` sentinel to produce undefined.
 */
export function produce<T>(base: T, recipe: (draft: T) => T | void): T {
  if (isDraftable(base)) {
    const draft = createDraft(base);
    const maybeNewValue = recipe(draft);
    // Type `T` might contains null, and we deliberately differentiate between
    // "undefined" and "null" here.
    // eslint-disable-next-line @typescript-eslint/prefer-nullish-coalescing
    return maybeNewValue !== undefined ? maybeNewValue : finalizeDraft(draft);
  } else {
    const maybeNewValue = recipe(base);
    // Type `T` might contains null, and we deliberately differentiate between
    // "undefined" and "null" here.
    // eslint-disable-next-line @typescript-eslint/prefer-nullish-coalescing
    return maybeNewValue !== undefined ? maybeNewValue : base;
  }
}
