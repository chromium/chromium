// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_service_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/default_top_sites_provider.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class ThumbnailServiceTest : public testing::Test {
  content::TestBrowserThreadBundle thread_bundle_;
};

namespace {

// A mock version of TopSitesImpl, used for testing
// ShouldAcquirePageThumbnail().
class MockTopSites : public history::TopSitesImpl {
 public:
  explicit MockTopSites(Profile* profile)
      : history::TopSitesImpl(
            profile->GetPrefs(),
            /*history_service=*/nullptr,
            std::make_unique<history::DefaultTopSitesProvider>(
                /*history_service=*/nullptr),
            history::PrepopulatedPageList(),
            base::Bind(CanAddURLToHistory)),
        capacity_(1) {}

  // history::TopSitesImpl overrides.
  bool IsNonForcedFull() override { return known_url_map_.size() >= capacity_; }
  bool IsForcedFull() override { return false; }
  bool IsKnownURL(const GURL& url) override {
    return known_url_map_.find(url.spec()) != known_url_map_.end();
  }
  bool GetPageThumbnailScore(const GURL& url,
                             history::ThumbnailScore* score) override {
    std::map<std::string, history::ThumbnailScore>::const_iterator iter =
        known_url_map_.find(url.spec());
    if (iter == known_url_map_.end()) {
      return false;
    } else {
      *score = iter->second;
      return true;
    }
  }

  // Adds a known URL with the associated thumbnail score.
  void AddKnownURL(const GURL& url, const history::ThumbnailScore& score) {
    known_url_map_[url.spec()] = score;
  }

 private:
  ~MockTopSites() override {}

  const size_t capacity_;
  std::map<std::string, history::ThumbnailScore> known_url_map_;

  DISALLOW_COPY_AND_ASSIGN(MockTopSites);
};

// Testing factory that build a |MockTopSites| instance.
scoped_refptr<RefcountedKeyedService> BuildMockTopSites(
    content::BrowserContext* profile) {
  return scoped_refptr<RefcountedKeyedService>(
      new MockTopSites(static_cast<Profile*>(profile)));
}

// A mock version of TestingProfile holds MockTopSites.
class MockProfile : public TestingProfile {
 public:
  MockProfile() {
    TopSitesFactory::GetInstance()->SetTestingFactory(
        this, base::BindRepeating(&BuildMockTopSites));
  }

  void AddKnownURL(const GURL& url, const history::ThumbnailScore& score) {
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(this);
    static_cast<MockTopSites*>(top_sites.get())->AddKnownURL(url, score);
  }

 private:

  DISALLOW_COPY_AND_ASSIGN(MockProfile);
};

}  // namespace

TEST_F(ThumbnailServiceTest, ShouldAcquireThumbnailOnlyForGoodUrl) {
  const GURL kGoodURL("http://www.google.com/");
  const GURL kBadURL("chrome://newtab");
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;

  // Set up the mock profile along with mock top sites.
  MockProfile profile;

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service(
      new thumbnails::ThumbnailServiceImpl(&profile));

  // Should be false because it's a bad URL.
  EXPECT_FALSE(
      thumbnail_service->ShouldAcquirePageThumbnail(kBadURL, transition));

  // Should be true, as it's a good URL.
  EXPECT_TRUE(
      thumbnail_service->ShouldAcquirePageThumbnail(kGoodURL, transition));
}

TEST_F(ThumbnailServiceTest, ShouldUpdateThumbnail) {
  const GURL kGoodURL("http://www.google.com/");
  const ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;

  // Set up the mock profile along with mock top sites.
  MockProfile profile;

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service(
      new thumbnails::ThumbnailServiceImpl(&profile));

  // Not checking incognito mode since the service wouldn't have been created
  // in that case anyway.

  // Add a known URL. This makes the top sites data full.
  history::ThumbnailScore bad_score;
  bad_score.time_at_snapshot = base::Time::UnixEpoch();  // Ancient time stamp.
  profile.AddKnownURL(kGoodURL, bad_score);
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(&profile);
  ASSERT_TRUE(top_sites->IsNonForcedFull());

  // Should be false, as the top sites data is full, and the new URL is
  // not known.
  const GURL kAnotherGoodURL("http://www.youtube.com/");
  EXPECT_FALSE(thumbnail_service->ShouldAcquirePageThumbnail(kAnotherGoodURL,
                                                             transition));

  // Should be true, as the existing thumbnail is bad (i.e. need a better one).
  EXPECT_TRUE(
      thumbnail_service->ShouldAcquirePageThumbnail(kGoodURL, transition));

  // Replace the thumbnail score with a really good one.
  history::ThumbnailScore good_score;
  good_score.time_at_snapshot = base::Time::Now();  // Very new.
  good_score.at_top = true;
  good_score.good_clipping = true;
  good_score.boring_score = 0.0;
  good_score.load_completed = true;
  profile.AddKnownURL(kGoodURL, good_score);

  // Should be false, as the existing thumbnail is good enough (i.e. don't
  // need to replace the existing thumbnail which is new and good).
  EXPECT_FALSE(
      thumbnail_service->ShouldAcquirePageThumbnail(kGoodURL, transition));
}

TEST_F(ThumbnailServiceTest,
       ShouldAcquireTempThumbnailDependingOnTransitionType) {
  const GURL kUnknownURL("http://www.google.com/");
  const ui::PageTransition interesting_transition = ui::PAGE_TRANSITION_TYPED;
  const ui::PageTransition uninteresting_transition = ui::PAGE_TRANSITION_LINK;

  // Set up the mock profile along with mock top sites.
  MockProfile profile;

  scoped_refptr<thumbnails::ThumbnailService> thumbnail_service(
      new thumbnails::ThumbnailServiceImpl(&profile));

  // Top sites isn't full. We should acquire thumbnails of unknown URLs if they
  // have an interesting page transition type.
  EXPECT_TRUE(thumbnail_service->ShouldAcquirePageThumbnail(
      kUnknownURL, interesting_transition));
  EXPECT_FALSE(thumbnail_service->ShouldAcquirePageThumbnail(
      kUnknownURL, uninteresting_transition));

  // Add a known URL. This makes the top sites data full.
  const GURL kKnownURL("http://www.known.com/");
  history::ThumbnailScore bad_score;
  bad_score.time_at_snapshot = base::Time::UnixEpoch();  // Ancient time stamp.
  profile.AddKnownURL(kKnownURL, bad_score);
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(&profile);
  ASSERT_TRUE(top_sites->IsNonForcedFull());

  // Now top sites is full, so we shouldn't acquire any thumbnails of unknown
  // URLs anymore, regardless of the transition type.
  EXPECT_FALSE(thumbnail_service->ShouldAcquirePageThumbnail(
      kUnknownURL, interesting_transition));
  EXPECT_FALSE(thumbnail_service->ShouldAcquirePageThumbnail(
      kUnknownURL, uninteresting_transition));

  // We should still take thumbnails of known URLs though, regardless of the
  // transition type.
  EXPECT_TRUE(thumbnail_service->ShouldAcquirePageThumbnail(
      kKnownURL, interesting_transition));
  EXPECT_TRUE(thumbnail_service->ShouldAcquirePageThumbnail(
      kKnownURL, uninteresting_transition));
}
