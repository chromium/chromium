// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOOKALIKES_LOOKALIKE_TEST_HELPER_H_
#define CHROME_BROWSER_LOOKALIKES_LOOKALIKE_TEST_HELPER_H_

// Helper methods for interstitial and safety tips lookalike tests.
// These allow the tests to use test data instead of prod, such as test top
// domain lists.
void SetUpLookalikeTestParams();
void TearDownLookalikeTestParams();

#endif
