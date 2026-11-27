// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_bookmark_iph_controller.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/dino/dino_features.h"
#include "chrome/browser/dino/dino_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace dino {

namespace {

constexpr char kOfflineExampleUrl[] = "https://offline-dino.example/";

class DinoBookmarkIphControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(BookmarkModelFactory::GetInstance(),
                              BookmarkModelFactory::GetDefaultFactory());
    profile_ = builder.Build();
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model());

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    web_contents_->WasShown();
  }

  void TearDown() override {
    if (PrefService* local_state =
            TestingBrowserProcess::GetGlobal()->local_state()) {
      local_state->SetBoolean(::prefs::kPromotionsEnabled, true);
    }
    web_contents_.reset();
    profile_.reset();
  }

  void NavigateToOfflineErrorPage(content::WebContents* contents,
                                  const GURL& url = GURL(kOfflineExampleUrl)) {
    auto navigation =
        content::NavigationSimulator::CreateBrowserInitiated(url, contents);
    navigation->Fail(net::ERR_INTERNET_DISCONNECTED);
    navigation->CommitErrorPage();
  }

  TestingProfile* profile() { return profile_.get(); }
  content::WebContents* web_contents() { return web_contents_.get(); }

  bookmarks::BookmarkModel* bookmark_model() {
    return BookmarkModelFactory::GetForBrowserContext(profile_.get());
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
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

TEST_F(DinoBookmarkIphControllerTest, CanShowOnEligibleOfflineErrorPage) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  NavigateToOfflineErrorPage(web_contents());

  EXPECT_TRUE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CanShowWhenDinoDelightDisabled) {
  NavigateToOfflineErrorPage(web_contents());

  EXPECT_TRUE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CannotShowWhenPromotionsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      ::prefs::kPromotionsEnabled, false);
  NavigateToOfflineErrorPage(web_contents());

  EXPECT_FALSE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CannotShowWhenTabIsHidden) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  NavigateToOfflineErrorPage(web_contents());
  web_contents()->WasHidden();

  EXPECT_FALSE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CannotShowOnNonErrorPage) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kOfflineExampleUrl));

  EXPECT_FALSE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CannotShowOnChromeDinoErrorPage) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  NavigateToOfflineErrorPage(web_contents(), GURL(kDinoUrl));

  EXPECT_FALSE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CannotShowForOffTheRecordProfile) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  std::unique_ptr<content::WebContents> otr_contents =
      content::WebContentsTester::CreateTestWebContents(otr_profile, nullptr);
  otr_contents->WasShown();
  NavigateToOfflineErrorPage(otr_contents.get());

  EXPECT_FALSE(CanShowDinoBookmarkPrompt(otr_contents.get()));
}

TEST_F(DinoBookmarkIphControllerTest, CannotShowWhenEditBookmarksDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  profile()->GetPrefs()->SetBoolean(bookmarks::prefs::kEditBookmarksEnabled,
                                    false);
  NavigateToOfflineErrorPage(web_contents());

  EXPECT_FALSE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CanShowWhenMaxTimesShownReached) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  profile()->GetPrefs()->SetInteger(
      prefs::kDinoBookmarkPromptMaxTimesShownCounter,
      kDinoBookmarkPromptMaxTimesShown);
  NavigateToOfflineErrorPage(web_contents());

  EXPECT_TRUE(CanShowDinoBookmarkPrompt(web_contents()));
}

TEST_F(DinoBookmarkIphControllerTest, CannotShowWhenAlreadyBookmarked) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  bookmark_model()->AddURL(bookmark_model()->bookmark_bar_node(), 0,
                           u"Dino Game", GURL(kDinoUrl));
  NavigateToOfflineErrorPage(web_contents());

  EXPECT_FALSE(CanShowDinoBookmarkPrompt(web_contents()));
}

}  // namespace

}  // namespace dino
