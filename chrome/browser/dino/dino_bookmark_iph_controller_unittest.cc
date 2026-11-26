// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_bookmark_iph_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/dino/dino_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace dino {

namespace {

class DinoBookmarkIphControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                              BookmarkModelFactory::GetDefaultFactory());
    profile_ = builder.Build();
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model());
  }

  TestingProfile* profile() { return profile_.get(); }

  bookmarks::BookmarkModel* bookmark_model() {
    return BookmarkModelFactory::GetForBrowserContext(profile_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(DinoBookmarkIphControllerTest, SavesBookmarkToBookmarkBarWhenEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  ASSERT_FALSE(bookmark_model()->IsBookmarked(GURL(kDinoUrl)));

  EXPECT_TRUE(SaveDinoBookmarkFromPromo(profile()));
  EXPECT_TRUE(bookmark_model()->IsBookmarked(GURL(kDinoUrl)));
}

TEST_F(DinoBookmarkIphControllerTest, DoesNotSaveWhenDinoDelightDisabled) {
  ASSERT_FALSE(bookmark_model()->IsBookmarked(GURL(kDinoUrl)));

  EXPECT_FALSE(SaveDinoBookmarkFromPromo(profile()));
  EXPECT_FALSE(bookmark_model()->IsBookmarked(GURL(kDinoUrl)));
}

TEST_F(DinoBookmarkIphControllerTest, DoesNotSaveWhenAlreadyBookmarked) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), 0,
                           u"Dino Game", GURL(kDinoUrl));

  EXPECT_FALSE(SaveDinoBookmarkFromPromo(profile()));
}

TEST_F(DinoBookmarkIphControllerTest, DoesNotSaveWhenEditBookmarksDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  profile()->GetPrefs()->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled,
                                    false);

  EXPECT_FALSE(SaveDinoBookmarkFromPromo(profile()));
  EXPECT_FALSE(bookmark_model()->IsBookmarked(GURL(kDinoUrl)));
}

TEST_F(DinoBookmarkIphControllerTest, DoesNotSaveForOffTheRecordProfile) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);

  EXPECT_FALSE(SaveDinoBookmarkFromPromo(otr_profile));
}

}  // namespace

}  // namespace dino
