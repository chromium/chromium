// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Temporary export until guest_query_receiver.js is converted to TS. The
 * generated .d.ts doesn't work due to an undefined/void mismatch.
 */
// eslint-disable-next-line @typescript-eslint/naming-convention
export function GUEST_TEST(
    testName: string, testCase: () => void|Promise<void>): void;
