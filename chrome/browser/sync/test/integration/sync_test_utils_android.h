// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_UTILS_ANDROID_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_UTILS_ANDROID_H_

// Utilities that interface with Java to support Sync testing on Android.

namespace sync_test_utils_android {

// Sets up the test account and signs in synchronously.
void SetUpAccountAndSignInForTesting();

// Sets up the test authentication environment synchronously using a worker
// thread.
//
// We recommend calling this function from the SetUp() method of the test
// fixture (e.g., CustomFixture::SetUp()) before calling the other SetUp()
// function down the stack (e.g., PlatformBrowserTest::SetUp()).
void SetUpAuthForTesting();

// Tears down the test authentication environment synchronously using a worker
// thread.
//
// We recommend calling this function from the PostRunTestOnMainThread() method
// of the test fixture, which allows multiple threads. See the following file
// for an example:
// chrome/browser/metrics/metrics_service_user_demographics_browsertest.cc.
void TearDownAuthForTesting();

}  // namespace sync_test_utils_android

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_UTILS_ANDROID_H_
