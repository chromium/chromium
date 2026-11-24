// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_bookmark_metrics_service.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/dino/dino_bookmark_metrics_service_factory.h"
#include "chrome/browser/dino/dino_metrics.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace dino {

namespace {

std::unique_ptr<TestingProfile> BuildProfileWithBookmarkMetrics(
    const base::FilePath& path = base::FilePath()) {
  TestingProfile::Builder builder;
  if (!path.empty()) {
    builder.SetPath(path);
  }
  builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                            BookmarkModelFactory::GetDefaultFactory());
  builder.AddTestingFactory(
      DinoBookmarkMetricsServiceFactory::GetInstance(),
      DinoBookmarkMetricsServiceFactory::GetDefaultFactory());
  return builder.Build();
}

}  // namespace

class DinoBookmarkMetricsServiceTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(DinoBookmarkMetricsServiceTest,
       RecordsDinoHasBookmarkFalseWhenNotBookmarked) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestingProfile> profile = BuildProfileWithBookmarkMetrics();
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile.get()));

  histogram_tester.ExpectUniqueSample(kDinoHasBookmarkHistogram, false, 1);
}

TEST_F(DinoBookmarkMetricsServiceTest,
       RecordsDinoHasBookmarkTrueWhenBookmarked) {
  base::ScopedTempDir profile_dir;
  ASSERT_TRUE(profile_dir.CreateUniqueTempDir());

  // Populate and persist a chrome://dino bookmark to the profile's storage.
  {
    std::unique_ptr<TestingProfile> initial_profile =
        BuildProfileWithBookmarkMetrics(profile_dir.GetPath());
    bookmarks::BookmarkModel* initial_model =
        BookmarkModelFactory::GetForBrowserContext(initial_profile.get());
    bookmarks::test::WaitForBookmarkModelToLoad(initial_model);
    initial_model->AddURL(initial_model->bookmark_bar_node(), 0, u"Dino",
                          GURL("chrome://dino"));
    initial_model->CommitPendingWriteForTest();
    task_environment_.FastForwardBy(base::Seconds(10));
  }

  // Reload the profile from disk and verify the baseline metric is true.
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestingProfile> reloaded_profile =
      BuildProfileWithBookmarkMetrics(profile_dir.GetPath());
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(reloaded_profile.get()));

  histogram_tester.ExpectUniqueSample(kDinoHasBookmarkHistogram, true, 1);
}

TEST_F(DinoBookmarkMetricsServiceTest, DoesNotRecordForOffTheRecordProfile) {
  std::unique_ptr<TestingProfile> profile = BuildProfileWithBookmarkMetrics();
  bookmarks::test::WaitForBookmarkModelToLoad(
      BookmarkModelFactory::GetForBrowserContext(profile.get()));

  base::HistogramTester histogram_tester;
  Profile* otr_profile =
      profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(nullptr,
            DinoBookmarkMetricsServiceFactory::GetForProfile(otr_profile));
  histogram_tester.ExpectTotalCount(kDinoHasBookmarkHistogram, 0);
}

TEST_F(DinoBookmarkMetricsServiceTest, RecordsWhenModelAlreadyLoaded) {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                            BookmarkModelFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = builder.Build();
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile.get());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  model->AddURL(model->bookmark_bar_node(), 0, u"Dino", GURL("chrome://dino"));

  base::HistogramTester histogram_tester;
  DinoBookmarkMetricsService service(model);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  histogram_tester.ExpectUniqueSample(kDinoHasBookmarkHistogram, true, 1);
  service.Shutdown();
}

TEST_F(DinoBookmarkMetricsServiceTest,
       DoesNotRecordIfShutdownBeforeLoadCompletes) {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                            BookmarkModelFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = builder.Build();
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(profile.get());
  ASSERT_FALSE(model->loaded());

  base::HistogramTester histogram_tester;
  DinoBookmarkMetricsService service(model);
  service.Shutdown();
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  histogram_tester.ExpectTotalCount(kDinoHasBookmarkHistogram, 0);
}

}  // namespace dino
