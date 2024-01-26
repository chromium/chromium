// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <set>

#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/policy/safe_search_policy_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/safe_search_api/safe_search_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

IN_PROC_BROWSER_TEST_F(SafeSearchPolicyTest, LegacySafeSearch) {
  static_assert(safe_search_api::YOUTUBE_RESTRICT_OFF == 0 &&
                    safe_search_api::YOUTUBE_RESTRICT_MODERATE == 1 &&
                    safe_search_api::YOUTUBE_RESTRICT_STRICT == 2 &&
                    safe_search_api::YOUTUBE_RESTRICT_COUNT == 3,
                "This test relies on mapping ints to enum values.");

  // Go over all combinations of (undefined, true, false) for the policies
  // ForceSafeSearch, ForceGoogleSafeSearch and ForceYouTubeSafetyMode as well
  // as (undefined, off, moderate, strict) for ForceYouTubeRestrict and make
  // sure the prefs are set as expected.
  const int num_restrict_modes = 1 + safe_search_api::YOUTUBE_RESTRICT_COUNT;
  for (int i = 0; i < 3 * 3 * 3 * num_restrict_modes; i++) {
    int val = i;
    int legacy_safe_search = val % 3;
    val /= 3;
    int google_safe_search = val % 3;
    val /= 3;
    int legacy_youtube = val % 3;
    val /= 3;
    int youtube_restrict = val % num_restrict_modes;

    // Override the default SafeSearch setting using policies.
    ApplySafeSearchPolicy(
        legacy_safe_search == 0
            ? std::nullopt
            : std::make_optional<base::Value>(legacy_safe_search == 1),
        google_safe_search == 0
            ? std::nullopt
            : std::make_optional<base::Value>(google_safe_search == 1),
        legacy_youtube == 0
            ? std::nullopt
            : std::make_optional<base::Value>(legacy_youtube == 1),
        youtube_restrict == 0
            ? std::nullopt  // subtracting 1 gives
                            // 0,1,2, see above
            : std::make_optional<base::Value>(youtube_restrict - 1));

    // The legacy ForceSafeSearch policy should only have an effect if none of
    // the other 3 policies are defined.
    bool legacy_safe_search_in_effect =
        google_safe_search == 0 && legacy_youtube == 0 &&
        youtube_restrict == 0 && legacy_safe_search != 0;
    bool legacy_safe_search_enabled =
        legacy_safe_search_in_effect && legacy_safe_search == 1;

    // Likewise, ForceYouTubeSafetyMode should only have an effect if
    // ForceYouTubeRestrict is not set.
    bool legacy_youtube_in_effect =
        youtube_restrict == 0 && legacy_youtube != 0;
    bool legacy_youtube_enabled =
        legacy_youtube_in_effect && legacy_youtube == 1;

    // Consistency check, can't have both legacy modes at the same time.
    EXPECT_FALSE(legacy_youtube_in_effect && legacy_safe_search_in_effect);

    // Google safe search can be triggered by the ForceGoogleSafeSearch policy
    // or the legacy safe search mode.
    PrefService* prefs = browser()->profile()->GetPrefs();
    EXPECT_EQ(google_safe_search != 0 || legacy_safe_search_in_effect,
              prefs->IsManagedPreference(
                  policy::policy_prefs::kForceGoogleSafeSearch));
    EXPECT_EQ(google_safe_search == 1 || legacy_safe_search_enabled,
              prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));

    // YouTube restrict mode can be triggered by the ForceYouTubeRestrict policy
    // or any of the legacy modes.
    EXPECT_EQ(youtube_restrict != 0 || legacy_safe_search_in_effect ||
                  legacy_youtube_in_effect,
              prefs->IsManagedPreference(
                  policy::policy_prefs::kForceYouTubeRestrict));

    if (youtube_restrict != 0) {
      // The ForceYouTubeRestrict policy should map directly to the pref.
      EXPECT_EQ(youtube_restrict - 1,
                prefs->GetInteger(policy::policy_prefs::kForceYouTubeRestrict));
    } else {
      // The legacy modes should result in MODERATE strictness, if enabled.
      safe_search_api::YouTubeRestrictMode expected_mode =
          legacy_safe_search_enabled || legacy_youtube_enabled
              ? safe_search_api::YOUTUBE_RESTRICT_MODERATE
              : safe_search_api::YOUTUBE_RESTRICT_OFF;
      EXPECT_EQ(prefs->GetInteger(policy::policy_prefs::kForceYouTubeRestrict),
                expected_mode);
    }
  }
}

IN_PROC_BROWSER_TEST_F(SafeSearchPolicyTest, ForceGoogleSafeSearch) {
  base::Lock lock;
  std::set<GURL> google_urls_requested;
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) -> bool {
        if (params->url_request.url.host() != "google.com")
          return false;
        base::AutoLock auto_lock(lock);
        google_urls_requested.insert(params->url_request.url);
        std::string relative_path("chrome/test/data/simple.html");
        content::URLLoaderInterceptor::WriteResponse(relative_path,
                                                     params->client.get());
        return true;
      }));

  // Verifies that requests to Google Search engine with the SafeSearch
  // enabled set the safe=active&ssui=on parameters at the end of the query.
  // First check that nothing happens.
  CheckSafeSearch(browser(), false);

  // Go over all combinations of (undefined, true, false) for the
  // ForceGoogleSafeSearch policy.
  for (int safe_search = 0; safe_search < 3; safe_search++) {
    // Override the Google safe search policy.
    ApplySafeSearchPolicy(
        std::nullopt,     // ForceSafeSearch
        safe_search == 0  // ForceGoogleSafeSearch
            ? std::nullopt
            : std::make_optional<base::Value>(safe_search == 1),
        std::nullopt,   // ForceYouTubeSafetyMode
        std::nullopt);  // ForceYouTubeRestrict
    // Verify that the safe search pref behaves the way we expect.
    PrefService* prefs = browser()->profile()->GetPrefs();
    EXPECT_EQ(safe_search != 0,
              prefs->IsManagedPreference(
                  policy::policy_prefs::kForceGoogleSafeSearch));
    EXPECT_EQ(safe_search == 1,
              prefs->GetBoolean(policy::policy_prefs::kForceGoogleSafeSearch));

    // Verify that safe search actually works.
    CheckSafeSearch(browser(), safe_search == 1);

    GURL google_url(GetExpectedSearchURL(safe_search == 1));

    {
      // Verify that the network request is what we expect.
      base::AutoLock auto_lock(lock);
      ASSERT_TRUE(google_urls_requested.find(google_url) !=
                  google_urls_requested.end());
      google_urls_requested.clear();
    }

    {
      // Now check subresource loads.
      FetchSubresource(browser()->tab_strip_model()->GetActiveWebContents(),
                       GURL("http://google.com/"));

      base::AutoLock auto_lock(lock);
      ASSERT_TRUE(google_urls_requested.find(google_url) !=
                  google_urls_requested.end());
    }
  }
}

}  // namespace policy
