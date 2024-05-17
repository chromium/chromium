// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/url_lookup_service.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/url_lookup_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/variations/pref_names.h"
#include "content/public/test/browser_test.h"

namespace safe_browsing {

class SafeBrowsingUrlLookupServiceTest : public InProcessBrowserTest {
 public:
  SafeBrowsingUrlLookupServiceTest() = default;
  SafeBrowsingUrlLookupServiceTest(const SafeBrowsingUrlLookupServiceTest&) =
      delete;
  SafeBrowsingUrlLookupServiceTest& operator=(
      const SafeBrowsingUrlLookupServiceTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(SafeBrowsingUrlLookupServiceTest,
                       ServiceRespectsLocationChanges) {
  safe_browsing::SetSafeBrowsingState(
      browser()->profile()->GetPrefs(),
      safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION);
  auto* url_lookup_service =
      RealTimeUrlLookupServiceFactory::GetForProfile(browser()->profile());

  // By default for ESB, full URL lookups should be enabled.
  EXPECT_TRUE(url_lookup_service->CanPerformFullURLLookup());

  // Changing to CN should disable the lookups.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "cn");
  EXPECT_FALSE(url_lookup_service->CanPerformFullURLLookup());

  // Changing to US should re-enable the lookups.
  g_browser_process->local_state()->SetString(
      variations::prefs::kVariationsCountry, "us");
  EXPECT_TRUE(url_lookup_service->CanPerformFullURLLookup());
}

}  // namespace safe_browsing
