// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper function to shallow-copy an object and replace some properties.
// Useful to convert from these private types to public types. This will fail to
// compile if a property is missed.
export function replaceProperties<O, R>(
    original: O, replacements: R): Omit<O, keyof R>&R {
  return Object.assign(Object.assign({}, original) as any, replacements);
}
