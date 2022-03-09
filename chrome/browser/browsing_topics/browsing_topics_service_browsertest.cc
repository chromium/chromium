// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

class BrowsingTopicsDisabledBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingTopicsDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{blink::features::kBrowsingTopics});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledBrowserTest,
                       NoBrowsingTopicsService) {
  EXPECT_FALSE(
      BrowsingTopicsServiceFactory::GetForProfile(browser()->profile()));
}

class BrowsingTopicsBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingTopicsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kBrowsingTopics},
        /*disabled_features=*/{});
  }

  browsing_topics::BrowsingTopicsService* browsing_topics_service() {
    return BrowsingTopicsServiceFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, HasBrowsingTopicsService) {
  EXPECT_TRUE(browsing_topics_service());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, NoServiceInIncognitoMode) {
  CreateIncognitoBrowser(browser()->profile());

  EXPECT_TRUE(browser()->profile()->HasPrimaryOTRProfile());

  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  EXPECT_TRUE(incognito_profile);

  BrowsingTopicsService* incognito_browsing_topics_service =
      BrowsingTopicsServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(incognito_browsing_topics_service);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, BrowsingTopicsStateOnStart) {
  EXPECT_TRUE(browsing_topics_service()
                  ->GetTopicsForSiteForDisplay(/*top_origin=*/url::Origin())
                  .empty());
  EXPECT_TRUE(browsing_topics_service()->GetTopTopicsForDisplay().empty());
}

}  // namespace browsing_topics
