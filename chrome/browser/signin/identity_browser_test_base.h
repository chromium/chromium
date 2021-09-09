// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_IDENTITY_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_SIGNIN_IDENTITY_BROWSER_TEST_BASE_H_

#include "build/chromeos_buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "services/network/test/test_url_loader_factory.h"

namespace content {
class BrowserMainParts;
}

// Base class for browser tests that depend on AccountManager on Lacros - e.g.
// tests that manage accounts by calling methods like
// `signin::MakePrimaryAccountAvailable` from identity_test_utils.
// TODO(https://crbug.com/982233): consider using this base class on Ash, and
// remove the initialization from profile_impl.
class IdentityBrowserTestBase : public InProcessBrowserTest {
 public:
  IdentityBrowserTestBase() = default;
  IdentityBrowserTestBase(const IdentityBrowserTestBase&) = delete;
  IdentityBrowserTestBase& operator=(const IdentityBrowserTestBase&) = delete;
  ~IdentityBrowserTestBase() override = default;

 protected:
  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override;
};

// When including either identity_browser_test_base.h or android_browser_test.h
// depending on the platform, use this type alias as the test base class.
using IdentityPlatformBrowserTest = IdentityBrowserTestBase;

#endif  // CHROME_BROWSER_SIGNIN_IDENTITY_BROWSER_TEST_BASE_H_
