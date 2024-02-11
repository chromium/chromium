// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** Reply to test messages. Contents depend on the test message sent. */
export interface TestMessageResponseData {
  testQueryResult: string;
  testQueryResultData?: any;
}

/** Data for the "run-test-case" Message Pipe message. */
export interface TestMessageRunTestCase {
  testCase: string;
}
