// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Force upcast a value to a specific type.
 *
 * See https://github.com/microsoft/TypeScript/issues/51853 .
 */
export function upcast<T>(x: T): T {
  return x;
}

/**
 * Force cast a value to a specific type.
 *
 * Usage should be accompany with comments on why this is safe.
 */
export function forceCast<T>(x: unknown): T {
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return x as unknown as T;
}

/*
 * Expand the type to allow extra key/value in the object.
 *
 * This is useful for assigning an object literal to a type with only defines
 * the type partially.
 */
export type RecursiveExtraKey<T> = T extends Record<string, unknown>?
  Record<string, unknown>&{[K in keyof T]: RecursiveExtraKey<T[K]>} :
  T extends unknown[] ? {[K in keyof T]: RecursiveExtraKey<T[K]>} : T;
