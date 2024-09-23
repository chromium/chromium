// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from './assert.js';

// Crockford's Base32.
const BASE = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';

function encodeTime(time: number): string {
  const chars: string[] = [];
  for (let i = 9; i >= 0; i--) {
    const rem = time % 32;
    chars.push(assertExists(BASE[rem]));
    time = (time - rem) / 32;
  }
  chars.reverse();
  return chars.join('');
}

function encodeRandom(): string {
  const arr = new Uint8Array(16);
  crypto.getRandomValues(arr);
  return Array.from(arr).map((x) => BASE[x % 32]).join('');
}

/**
 * Generates a unique ulid.
 * See https://github.com/ulid/spec for more details.
 */
export function ulid(): string {
  const now = Date.now();
  return encodeTime(now) + encodeRandom();
}
