// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * TypeScript definitions for symbols defined in test_api.js that are still used
 * by test files converted from the old js2gtest framework. This only lists
 * methods still used by help_app until they are converted to modern
 * equivalents.
 */

declare function assertEquals(
    expected: any, actual: any, message?: string): void;
declare function assertDeepEquals(lhs: any, rhs: any): void;

interface ChaiJsAsserts {
  isTrue(value: any): void;
  match(string: string, regex: RegExp, message?: string): void;
  sameDeepMembers(lhs: any, rhs: any): void;
}

interface ChaiJs {
  assert: ChaiJsAsserts;
  expect: ChaiJsAsserts;
}

declare const chai: ChaiJs;
