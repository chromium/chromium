// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_promotion_source_navigation_observer.h"

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

namespace glic {

class GlicPromotionSourceNavigationObserverBrowserTest
    : public GlicBrowserTest {
 public:
  GlicPromotionSourceNavigationObserverBrowserTest() {
    marketing_server_.AddDefaultHandlers(GetChromeTestDataDir());
  }

  void SetUp() override {
    ASSERT_TRUE(marketing_server_.InitializeAndListen());
    GlicBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    marketing_server_.StartAcceptingConnections();
    GlicPromotionSourceNavigationObserver::SetPromotionPageUrlForTesting(
        marketing_server_.GetURL("/title1.html"));
    GlicBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    GlicPromotionSourceNavigationObserver::SetPromotionPageUrlForTesting(
        GURL());
    GlicBrowserTest::TearDownOnMainThread();
  }

  bool OpenTabWithLink(const GURL& url) {
    NavigateParams params(GetBrowser(), url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);
    return params.navigated_or_inserted_contents != nullptr;
  }

 protected:
  net::test_server::EmbeddedTestServer marketing_server_;
};

IN_PROC_BROWSER_TEST_F(GlicPromotionSourceNavigationObserverBrowserTest,
                       PromotionSourceWebstoreAttribution) {
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());

  GURL webstore_url = marketing_server_.GetURL(
      "/title1.html?utm_source=owned&utm_medium=chrome-web-store&utm_campaign="
      "GiC-promo");
  EXPECT_TRUE(OpenTabWithLink(webstore_url));

  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceWebstore);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceWebstore));
}

IN_PROC_BROWSER_TEST_F(GlicPromotionSourceNavigationObserverBrowserTest,
                       PromotionSourceChromeDotComAttribution) {
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());

  // Benign query parameters like ?hl=fr should still attribute to ChromeDotCom
  // when utm_medium and source are absent.
  GURL chrome_dot_com_url = marketing_server_.GetURL("/title1.html?hl=fr");
  EXPECT_TRUE(OpenTabWithLink(chrome_dot_com_url));

  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceChromeDotCom);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceChromeDotCom));
}

IN_PROC_BROWSER_TEST_F(GlicPromotionSourceNavigationObserverBrowserTest,
                       PromotionSourceZssAttribution) {
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());

  GURL zss_url = marketing_server_.GetURL("/title1.html?source=zss");
  EXPECT_TRUE(OpenTabWithLink(zss_url));

  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceZss);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(kGlicPromotionSourceTrialName,
                                                  kGlicPromotionSourceZss));
}

IN_PROC_BROWSER_TEST_F(GlicPromotionSourceNavigationObserverBrowserTest,
                       UnrecognizedQueryDoesNotAttributePromotionSource) {
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());

  // Navigations with unrecognized utm_medium or source values must not
  // attribute.
  GURL unrecognized_utm =
      marketing_server_.GetURL("/title1.html?utm_medium=other");
  EXPECT_TRUE(OpenTabWithLink(unrecognized_utm));
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());

  GURL unrecognized_source =
      marketing_server_.GetURL("/title1.html?source=other");
  EXPECT_TRUE(OpenTabWithLink(unrecognized_source));
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());
}

IN_PROC_BROWSER_TEST_F(GlicPromotionSourceNavigationObserverBrowserTest,
                       PromotionSourceMultipleAttribution) {
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());

  // First touch: visit from chrome.com (link navigation without query params).
  GURL chrome_dot_com_url = marketing_server_.GetURL("/title1.html");
  EXPECT_TRUE(OpenTabWithLink(chrome_dot_com_url));

  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceChromeDotCom);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceChromeDotCom));

  // Visiting the same source again should not change the tag.
  EXPECT_TRUE(OpenTabWithLink(chrome_dot_com_url));
  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceChromeDotCom);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceChromeDotCom));

  // Visiting an unrecognized campaign should not change the existing tag.
  GURL unrecognized_url =
      marketing_server_.GetURL("/title1.html?utm_medium=other");
  EXPECT_TRUE(OpenTabWithLink(unrecognized_url));
  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceChromeDotCom);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceChromeDotCom));

  // Visiting a different recognized source (webstore) tags the user as
  // "Multiple".
  GURL webstore_url = marketing_server_.GetURL(
      "/title1.html?utm_source=owned&utm_medium=chrome-web-store&utm_campaign="
      "GiC-promo");
  EXPECT_TRUE(OpenTabWithLink(webstore_url));

  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceMultiple);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceMultiple));
  EXPECT_FALSE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceChromeDotCom));
  EXPECT_FALSE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceWebstore));

  // Subsequent visits to any source (including zss) keep the user in
  // "Multiple".
  GURL zss_url = marketing_server_.GetURL("/title1.html?source=zss");
  EXPECT_TRUE(OpenTabWithLink(zss_url));
  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceMultiple);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceMultiple));
}

IN_PROC_BROWSER_TEST_F(GlicPromotionSourceNavigationObserverBrowserTest,
                       PRE_PromotionSourcePersistsAcrossRestart) {
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetString(prefs::kGlicPromotionSourceCohort).empty());

  GURL webstore_url = marketing_server_.GetURL(
      "/title1.html?utm_source=owned&utm_medium=chrome-web-store&utm_campaign="
      "GiC-promo");
  EXPECT_TRUE(OpenTabWithLink(webstore_url));

  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceWebstore);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceWebstore));
}

IN_PROC_BROWSER_TEST_F(GlicPromotionSourceNavigationObserverBrowserTest,
                       PromotionSourcePersistsAcrossRestart) {
  // Ensure the GlicKeyedService is active for the profile.
  ASSERT_TRUE(GlicKeyedService::Get(GetProfile()));

  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_EQ(prefs->GetString(prefs::kGlicPromotionSourceCohort),
            kGlicPromotionSourceWebstore);
  EXPECT_TRUE(variations::IsInSyntheticTrialGroup(
      kGlicPromotionSourceTrialName, kGlicPromotionSourceWebstore));
}

}  // namespace glic
