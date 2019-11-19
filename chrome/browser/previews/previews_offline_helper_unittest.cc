// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_offline_helper.h"

#include <memory>
#include <string>
#include <vector>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/offline_pages/request_coordinator_factory.h"
#include "chrome/browser/offline_pages/test_offline_page_model_builder.h"
#include "chrome/browser/offline_pages/test_request_coordinator_builder.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/offline_page_test_archiver.h"
#include "components/prefs/testing_pref_service.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

class PreviewsOfflineHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void TearDown() override {
    if (helper_)
      helper_->Shutdown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PreviewsOfflineHelper* NewHelper(content::BrowserContext* browser_context) {
    helper_.reset(new PreviewsOfflineHelper(browser_context));
    return helper_.get();
  }

  offline_pages::OfflinePageItem MakeAddedPageItem(
      const std::string& url,
      const std::string& original_url,
      const base::Time& creation_time) {
    offline_pages::OfflinePageItem item;
    item.url = GURL(url);
    item.original_url_if_different = GURL(original_url);
    item.creation_time = creation_time;
    return item;
  }

 private:
  std::unique_ptr<PreviewsOfflineHelper> helper_;
};

TEST_F(PreviewsOfflineHelperTest, TestAddRemovePages) {
  struct TestCase {
    std::string msg;
    bool enable_feature;
    std::vector<std::string> add_fresh_pages;
    std::vector<std::string> add_expired_pages;
    std::vector<std::string> want_pages;
    std::vector<std::string> not_want_pages;
    std::string original_url;
    size_t want_pref_size;
  };
  const TestCase kTestCases[]{
      {
          .msg = "All pages should return true when the feature is disabled",
          .enable_feature = false,
          .add_fresh_pages = {},
          .add_expired_pages = {},
          .want_pages = {"http://chromium.org"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 0,
      },
      {
          .msg = "Unknown page returns false",
          .enable_feature = true,
          .add_fresh_pages = {},
          .add_expired_pages = {},
          .want_pages = {},
          .not_want_pages = {"http://chromium.org"},
          .original_url = "",
          .want_pref_size = 0,
      },
      {
          .msg = "Fresh page returns true",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {},
          .want_pages = {"http://chromium.org"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 1,
      },
      {
          .msg = "Fresh page with the original URL returns true",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {},
          .want_pages = {"http://google.com"},
          .not_want_pages = {},
          .original_url = "http://google.com",
          .want_pref_size = 2,
      },
      {
          .msg = "Expired page returns false",
          .enable_feature = true,
          .add_fresh_pages = {},
          .add_expired_pages = {"http://chromium.org"},
          .want_pages = {},
          .not_want_pages = {"http://chromium.org"},
          .original_url = "",
          .want_pref_size = 0,
      },
      {
          .msg = "Expired then refreshed page returns true",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {"http://chromium.org"},
          .want_pages = {"http://chromium.org"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 1,
      },
      {
          .msg = "URL Fragments don't matter",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org"},
          .add_expired_pages = {},
          .want_pages = {"http://chromium.org",
                         "http://chromium.org/#previews"},
          .not_want_pages = {},
          .original_url = "",
          .want_pref_size = 1,
      },
      {
          .msg = "URLs with paths are different",
          .enable_feature = true,
          .add_fresh_pages = {"http://chromium.org/fresh"},
          .add_expired_pages = {"http://chromium.org/old"},
          .want_pages = {"http://chromium.org/fresh"},
          .not_want_pages = {"http://chromium.org/old"},
          .original_url = "",
          .want_pref_size = 1,
      },
  };

  base::Time fresh = base::Time::Now();
  base::Time expired = fresh -
                       previews::params::OfflinePreviewFreshnessDuration() -
                       base::TimeDelta::FromHours(1);

  const char kDictKey[] = "previews.offline_helper.available_pages";

  for (const TestCase& test_case : kTestCases) {
    SCOPED_TRACE(test_case.msg);

    base::HistogramTester histogram_tester;

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        previews::features::kOfflinePreviewsFalsePositivePrevention,
        test_case.enable_feature);

    TestingPrefServiceSimple test_prefs;
    PreviewsOfflineHelper::RegisterProfilePrefs(test_prefs.registry());

    PreviewsOfflineHelper* helper = NewHelper(nullptr);
    helper->SetPrefServiceForTesting(&test_prefs);

    // The tests above rely on this ordering.
    for (const std::string& expired_page : test_case.add_expired_pages) {
      helper->OfflinePageAdded(
          nullptr,
          MakeAddedPageItem(expired_page, test_case.original_url, expired));
    }
    for (const std::string& fresh_page : test_case.add_fresh_pages) {
      helper->OfflinePageAdded(
          nullptr,
          MakeAddedPageItem(fresh_page, test_case.original_url, fresh));
    }

    EXPECT_EQ(test_prefs.GetDictionary(kDictKey)->size(),
              test_case.want_pref_size);

    for (const std::string want : test_case.want_pages) {
      EXPECT_TRUE(helper->ShouldAttemptOfflinePreview(GURL(want)));
    }

    for (const std::string not_want : test_case.not_want_pages) {
      EXPECT_FALSE(helper->ShouldAttemptOfflinePreview(GURL(not_want)));
    }

    histogram_tester.ExpectTotalCount(
        "Previews.Offline.FalsePositivePrevention.Allowed",
        test_case.enable_feature
            ? test_case.not_want_pages.size() + test_case.want_pages.size()
            : 0);

    if (test_case.enable_feature && test_case.not_want_pages.size() > 0) {
      histogram_tester.ExpectBucketCount(
          "Previews.Offline.FalsePositivePrevention.Allowed", false,
          test_case.not_want_pages.size());
    }
    if (test_case.enable_feature && test_case.want_pages.size() > 0) {
      histogram_tester.ExpectBucketCount(
          "Previews.Offline.FalsePositivePrevention.Allowed", true,
          test_case.want_pages.size());
    }
  }
}

TEST_F(PreviewsOfflineHelperTest, TestMaxPrefSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      previews::features::kOfflinePreviewsFalsePositivePrevention,
      {{"max_pref_entries", "1"}});

  PreviewsOfflineHelper* helper = NewHelper(nullptr);

  base::Time first = base::Time::Now();
  base::Time second = first + base::TimeDelta::FromMinutes(1);

  helper->OfflinePageAdded(
      nullptr, MakeAddedPageItem("http://test.first.com", "", first));
  EXPECT_TRUE(
      helper->ShouldAttemptOfflinePreview(GURL("http://test.first.com")));

  helper->OfflinePageAdded(
      nullptr, MakeAddedPageItem("http://test.second.com", "", second));
  EXPECT_FALSE(
      helper->ShouldAttemptOfflinePreview(GURL("http://test.first.com")));
  EXPECT_TRUE(
      helper->ShouldAttemptOfflinePreview(GURL("http://test.second.com")));
}

TEST_F(PreviewsOfflineHelperTest, TestUpdateAllPrefEntries) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      previews::features::kOfflinePreviewsFalsePositivePrevention);

  base::HistogramTester histogram_tester;

  PreviewsOfflineHelper* helper = NewHelper(nullptr);
  base::Time now = base::Time::Now();

  helper->OfflinePageAdded(nullptr,
                           MakeAddedPageItem("http://cleared.com", "", now));
  EXPECT_TRUE(helper->ShouldAttemptOfflinePreview(GURL("http://cleared.com")));

  helper->UpdateAllPrefEntries({MakeAddedPageItem("http://new.com", "", now),
                                MakeAddedPageItem("http://new2.com", "", now)});
  EXPECT_FALSE(helper->ShouldAttemptOfflinePreview(GURL("http://cleared.com")));
  EXPECT_TRUE(helper->ShouldAttemptOfflinePreview(GURL("http://new.com")));
  EXPECT_TRUE(helper->ShouldAttemptOfflinePreview(GURL("http://new2.com")));

  histogram_tester.ExpectUniqueSample(
      "Previews.Offline.FalsePositivePrevention.PrefSize", 2, 1);
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)

class PreviewsOfflinePagesIntegrationTest
    : public PreviewsOfflineHelperTest,
      public offline_pages::OfflinePageTestArchiver::Observer {
 public:
  void SetUp() override {
    PreviewsOfflineHelperTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        previews::features::kOfflinePreviewsFalsePositivePrevention);

    // Sets up the factories for testing.
    offline_pages::OfflinePageModelFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile()->GetProfileKey(),
            base::BindRepeating(&offline_pages::BuildTestOfflinePageModel));
    base::RunLoop().RunUntilIdle();
    offline_pages::RequestCoordinatorFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(&offline_pages::BuildTestRequestCoordinator));
    base::RunLoop().RunUntilIdle();

    model_ = offline_pages::OfflinePageModelFactory::GetForBrowserContext(
        browser_context());
  }

  void NavigateAndCommit(const GURL& url) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
  }

  std::unique_ptr<offline_pages::OfflinePageArchiver> CreatePageArchiver(
      content::WebContents* web_contents) {
    std::unique_ptr<offline_pages::OfflinePageTestArchiver> archiver(
        new offline_pages::OfflinePageTestArchiver(
            this, web_contents->GetLastCommittedURL(),
            offline_pages::OfflinePageArchiver::ArchiverResult::
                SUCCESSFULLY_CREATED,
            base::string16(), 1234, std::string(),
            base::ThreadTaskRunnerHandle::Get()));
    return std::move(archiver);
  }

  void SavePage(content::WebContents* web_contents) {
    offline_pages::OfflinePageModel::SavePageParams save_page_params;
    save_page_params.url = web_contents->GetLastCommittedURL();
    save_page_params.client_id = offline_pages::ClientId("default", "id");
    save_page_params.proposed_offline_id = 4321;
    save_page_params.is_background = false;
    save_page_params.original_url = web_contents->GetLastCommittedURL();

    model_->SavePage(save_page_params, CreatePageArchiver(web_contents),
                     web_contents, base::DoNothing());
  }

  // OfflinePageTestArchiver::Observer:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  offline_pages::OfflinePageModel* model_;
};

TEST_F(PreviewsOfflinePagesIntegrationTest, TestOfflinePagesDBQuery) {
  GURL url("http://test.com/");
  NavigateAndCommit(url);
  SavePage(web_contents());
  base::RunLoop().RunUntilIdle();

  PreviewsOfflineHelper* helper = NewHelper(browser_context());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper->ShouldAttemptOfflinePreview(url));
  EXPECT_FALSE(helper->ShouldAttemptOfflinePreview(GURL("http://other.com")));
}

// This test checks that expired entries are not queried and populated in the
// pref. Since creating a stale offline page with this infrastructure is tricky,
// we instead set the freshness duration to negative to make any newly saved
// offline page stale.
TEST_F(PreviewsOfflinePagesIntegrationTest, TestOfflinePagesDBQuery_Expired) {
  ASSERT_TRUE(base::AssociateFieldTrialParams(
      "ClientSidePreviews", "Enabled",
      {{"offline_preview_freshness_duration_in_days", "-1"}}));
  ASSERT_TRUE(
      base::FieldTrialList::CreateFieldTrial("ClientSidePreviews", "Enabled"));

  GURL url("http://test.com/");
  NavigateAndCommit(url);
  SavePage(web_contents());
  base::RunLoop().RunUntilIdle();

  PreviewsOfflineHelper* helper = NewHelper(browser_context());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(helper->ShouldAttemptOfflinePreview(url));
}

#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)
