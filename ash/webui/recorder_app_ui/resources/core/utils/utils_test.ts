// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Run by `npx tsx utils_test.ts`

import {strict as assert} from 'node:assert';
import {describe, it} from 'node:test';

import {getWordCount, sleep} from './utils.js';

describe('sleep', () => {
  it('should sleep 1000 ms', async () => {
    const start = Date.now();
    await sleep(1000);
    const duration = Date.now() - start;
    assert(duration >= 1000);
  });
});

describe('getWordCount', () => {
  it(
    'should return 0 for empty string',
    () => {
      assert.equal(getWordCount('', 'en-US'), 0);
    },
  );

  it(
    'should count \'hello world\' as 2 words',
    () => {
      assert(getWordCount('hello world', 'en-US'), 2);
    },
  );
});
