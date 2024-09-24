// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_UTILS_ANDROID_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_UTILS_ANDROID_H_

#include <string>

// Utilities that interface with Java to support Sync testing on Android.

namespace sync_test_utils_android {

// Sets up the test account and signs in synchronously.
void SetUpAccountAndSignInForTesting();

// Sets up the test account, signs in, and enables Sync-the-feature
// synchronously.
void SetUpAccountAndSignInAndEnableSyncForTesting();

// Signs out and clears the primary account.
void SignOutForTesting();

// Sets up the fake authentication environment synchronously using a worker
// thread.
//
// We recommend calling this function from the SetUp() method of the test
// fixture (e.g., CustomFixture::SetUp()) before calling the other SetUp()
// function down the stack (e.g., PlatformBrowserTest::SetUp()).
void SetUpFakeAuthForTesting();

// Tears down the fake authentication environment synchronously using a worker
// thread.
//
// We recommend calling this function from the PostRunTestOnMainThread() method
// of the test fixture, which allows multiple threads. See the following file
// for an example:
// chrome/browser/metrics/metrics_service_user_demographics_browsertest.cc.
void TearDownFakeAuthForTesting();

// Sets up an account with given username and password, signs in synchronously
// on the live server.
void SetUpLiveAccountAndSignInForTesting(const std::string& username,
                                         const std::string& password);

// Sets up an account with given username and password, signs in, and enable
// Sync-the-feature synchronously on the live server.
void SetUpLiveAccountAndSignInAndEnableSyncForTesting(
    const std::string& username,
    const std::string& password);

// Shuts down the live authentication environment. Blocks until all pending
// token requests are finished.
//
// Should be called from PostRunTestOnMainThread() method of the test fixture.
void ShutdownLiveAuthForTesting();

}  // namespace sync_test_utils_android

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_UTILS_ANDROID_H_
