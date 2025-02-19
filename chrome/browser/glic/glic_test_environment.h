// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TEST_ENVIRONMENT_H_
#define CHROME_BROWSER_GLIC_GLIC_TEST_ENVIRONMENT_H_

#include "base/memory/weak_ptr.h"

class Profile;
namespace glic {
namespace internal {
class TestCookieSynchronizer;
}

// Overrides some glic functionality to allow tests that depend on glic to run.
// This should be created on the main thread.
// If possible, use InteractiveGlicTest instead of this directly!
// This class is used by tests in browser_tests and interactive_ui_tests that
// cannot use InteractiveGlicTest.
//
// Note: This constructs the GlicKeyedService, if it's not already created,
// which will also construct dependencies like IdentityManager. You likely want
// to create GlicTestEnvironment only after other test environment classes, like
// IdentityTestEnvironmentProfileAdaptor.
class GlicTestEnvironment {
 public:
  explicit GlicTestEnvironment(Profile* profile);
  ~GlicTestEnvironment();

  // Glic syncs sign-in cookies to the webview before showing the window. By
  // default, this class replaces this step with an immediately fake success.
  // Change the result of this operation here.
  void SetResultForFutureCookieSyncRequests(bool result);

 private:
  // Null during teardown.
  base::WeakPtr<internal::TestCookieSynchronizer> cookie_synchronizer_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TEST_ENVIRONMENT_H_
