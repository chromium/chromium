// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {helloWorld} from 'chrome://resources/js/hello_world.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';

export function helloWorldFromTS(): string {
  // <if expr="chromeos">
  return helloWorld() + ' from CrOS TypeScript!';
  // </if>

  return helloWorld() + ' from TypeScript!';
}

function main(): void {
  // Try assert, getDeepActiveElement.
  assert(getDeepActiveElement() === document.body);

  // Try helloWorldFromTS.
  document.body.innerText = helloWorldFromTS();

  // Try PromiseResolver with a string.
  const stringResolver = new PromiseResolver<string>();
  stringResolver.promise.then((result: string) => {
    assert(result.includes('done'));
  });
  stringResolver.resolve('done');

  // Try PromiseResolver with a number.
  const numberResolver = new PromiseResolver<number>();
  numberResolver.promise.then((result: number) => {
    assert(Math.min(result, 0) === 0);
  });
  numberResolver.resolve(5);
}

window.addEventListener('DOMContentLoaded', main);
