// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

namespace browsing_data_counter_utils {

class BrowsingDataCounterUtilsBrowserTest : public SyncTest {
 public:
  BrowsingDataCounterUtilsBrowserTest() : SyncTest(SINGLE_CLIENT) {}

  BrowsingDataCounterUtilsBrowserTest(
      const BrowsingDataCounterUtilsBrowserTest&) = delete;
  BrowsingDataCounterUtilsBrowserTest& operator=(
      const BrowsingDataCounterUtilsBrowserTest&) = delete;

  ~BrowsingDataCounterUtilsBrowserTest() override = default;
};

// TODO(crbug.com/40935822): Test is flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ShouldShowCookieException DISABLED_ShouldShowCookieException
#else
#define MAYBE_ShouldShowCookieException ShouldShowCookieException
#endif
IN_PROC_BROWSER_TEST_F(BrowsingDataCounterUtilsBrowserTest,
                       MAYBE_ShouldShowCookieException) {
  ASSERT_TRUE(SetupClients());

  // By default, a fresh profile is not signed in, nor syncing, so no cookie
  // exception should be shown.
  EXPECT_FALSE(ShouldShowCookieException(GetProfile(0)));

  // Sign the profile in.
  EXPECT_TRUE(GetClient(0)->SignInPrimaryAccount());

  // Sign-in alone shouldn't lead to a cookie exception.
  EXPECT_FALSE(ShouldShowCookieException(GetProfile(0)));

  // Enable sync.
  EXPECT_TRUE(GetClient(0)->SetupSync());

  // Now that we're syncing, we should offer to retain the cookie.
  EXPECT_TRUE(ShouldShowCookieException(GetProfile(0)));

#if !BUILDFLAG(IS_CHROMEOS)
  // Pause sync.
  GetClient(0)->SignOutPrimaryAccount();

  // There's no point in showing the cookie exception.
  EXPECT_FALSE(ShouldShowCookieException(GetProfile(0)));
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace browsing_data_counter_utils
