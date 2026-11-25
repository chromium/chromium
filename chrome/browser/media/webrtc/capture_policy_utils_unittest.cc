// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include <memory>

#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_type.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {
constexpr char kTestSite1[] = "https://foo.test.org";
constexpr char kTestSite1Pattern[] = "foo.test.org";
constexpr char kTestSite1NonMatchingPattern[] = "foo.org";
}  // namespace

class CapturePolicyUtilsTest : public testing::Test {
 public:
  CapturePolicyUtilsTest() {
    prefs_.registry()->RegisterBooleanPref(prefs::kScreenCaptureAllowed, true);
    prefs_.registry()->RegisterListPref(prefs::kScreenCaptureAllowedByOrigins);
    prefs_.registry()->RegisterListPref(prefs::kWindowCaptureAllowedByOrigins);
    prefs_.registry()->RegisterListPref(prefs::kTabCaptureAllowedByOrigins);
    prefs_.registry()->RegisterListPref(
        prefs::kSameOriginTabCaptureAllowedByOrigins);
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

 private:
  TestingPrefServiceSimple prefs_;
};

// Test that the default policy allows all capture.
TEST_F(CapturePolicyUtilsTest, SimpleAllowTest) {
  EXPECT_EQ(AllowedScreenCaptureLevel::kUnrestricted,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that setting |kScreenCaptureAllowed| to false, denies all capture.
TEST_F(CapturePolicyUtilsTest, SimpleDenyTest) {
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  EXPECT_EQ(AllowedScreenCaptureLevel::kDisallowed,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the FullCapture allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideUnrestricted) {
  base::ListValue matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kScreenCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kUnrestricted,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Window/Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideWindowTabs) {
  base::ListValue matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kWindowCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kWindow,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideTabs) {
  base::ListValue matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kTab,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Same Origin Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideSameOriginTabs) {
  base::ListValue matchlist;
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kSameOrigin,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that an item that doesn't match any list still respects the default.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideNoMatches) {
  base::ListValue matchlist;
  matchlist.Append(kTestSite1NonMatchingPattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kDisallowed,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Ensure that a full wildcard policy is accepted.
TEST_F(CapturePolicyUtilsTest, TestWildcard) {
  base::ListValue matchlist;
  matchlist.Append("*");
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(matchlist));
  EXPECT_EQ(AllowedScreenCaptureLevel::kTab,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://a.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://b.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://c.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://d.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://e.com"), *prefs()));
}

// Ensure that if a URL appears in multiple lists that it returns the most
// restrictive list that it is included in.
TEST_F(CapturePolicyUtilsTest, TestOverrideMoreRestrictive) {
  base::ListValue full_capture_list;
  full_capture_list.Append("a.com");
  full_capture_list.Append("b.com");
  full_capture_list.Append("c.com");
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->SetList(prefs::kScreenCaptureAllowedByOrigins,
                   std::move(full_capture_list));

  base::ListValue window_tab_list;
  window_tab_list.Append("b.com");
  prefs()->SetList(prefs::kWindowCaptureAllowedByOrigins,
                   std::move(window_tab_list));

  base::ListValue tab_list;
  tab_list.Append("c.com");
  tab_list.Append("d.com");
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(tab_list));

  base::ListValue same_origin_tab_list;
  same_origin_tab_list.Append("d.com");
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(same_origin_tab_list));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kUnrestricted,
      capture_policy::GetAllowedCaptureLevel(GURL("https://a.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kWindow,
      capture_policy::GetAllowedCaptureLevel(GURL("https://b.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kTab,
      capture_policy::GetAllowedCaptureLevel(GURL("https://c.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kSameOrigin,
      capture_policy::GetAllowedCaptureLevel(GURL("https://d.com"), *prefs()));
  EXPECT_EQ(
      AllowedScreenCaptureLevel::kDisallowed,
      capture_policy::GetAllowedCaptureLevel(GURL("https://e.com"), *prefs()));
}

// Ensure that if a URL appears in multiple lists that it returns the most
// restrictive list that it is included in.
TEST_F(CapturePolicyUtilsTest, TestSubdomainOverrides) {
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);

  base::ListValue same_origin_tab_list;
  same_origin_tab_list.Append("github.io");
  prefs()->SetList(prefs::kSameOriginTabCaptureAllowedByOrigins,
                   std::move(same_origin_tab_list));

  base::ListValue tab_list;
  tab_list.Append("foo.github.io");
  prefs()->SetList(prefs::kTabCaptureAllowedByOrigins, std::move(tab_list));

  base::ListValue full_capture_list;
  full_capture_list.Append("[*.]github.io");
  prefs()->SetList(prefs::kScreenCaptureAllowedByOrigins,
                   std::move(full_capture_list));

  EXPECT_EQ(AllowedScreenCaptureLevel::kSameOrigin,
            capture_policy::GetAllowedCaptureLevel(GURL("https://github.io"),
                                                   *prefs()));
  EXPECT_EQ(AllowedScreenCaptureLevel::kTab,
            capture_policy::GetAllowedCaptureLevel(
                GURL("https://foo.github.io"), *prefs()));
  EXPECT_EQ(AllowedScreenCaptureLevel::kUnrestricted,
            capture_policy::GetAllowedCaptureLevel(
                GURL("https://bar.github.io"), *prefs()));
}

// Returns a std::vector<DesktopMediaList::Type> containing all values.
std::vector<DesktopMediaList::Type> GetFullMediaList() {
  return {DesktopMediaList::Type::kCurrentTab,
          DesktopMediaList::Type::kWebContents, DesktopMediaList::Type::kWindow,
          DesktopMediaList::Type::kScreen};
}

// Test FilterMediaList with the different values of AllowedScreenCaptureLevel
// and ensure that values are filtered out and remain in sorted out.
TEST_F(CapturePolicyUtilsTest, FilterMediaListUnrestricted) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab, DesktopMediaList::Type::kWebContents,
      DesktopMediaList::Type::kWindow, DesktopMediaList::Type::kScreen};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kUnrestricted;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

TEST_F(CapturePolicyUtilsTest, FilterMediaListRestrictedWindow) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab, DesktopMediaList::Type::kWebContents,
      DesktopMediaList::Type::kWindow};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kWindow;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

TEST_F(CapturePolicyUtilsTest, FilterMediaListRestrictedTab) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab,
      DesktopMediaList::Type::kWebContents};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kTab;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

// We don't do the SameOrigin filter at the MediaTypes level, so this should be
// the same.
TEST_F(CapturePolicyUtilsTest, FilterMediaListRestrictedSameOrigin) {
  std::vector<DesktopMediaList::Type> expected_media_types = {
      DesktopMediaList::Type::kCurrentTab,
      DesktopMediaList::Type::kWebContents};

  AllowedScreenCaptureLevel level = AllowedScreenCaptureLevel::kSameOrigin;
  std::vector<DesktopMediaList::Type> actual_media_types = GetFullMediaList();
  capture_policy::FilterMediaList(actual_media_types, level);

  EXPECT_EQ(expected_media_types, actual_media_types);
}

TEST_F(CapturePolicyUtilsTest, IsMediaTypeAllowed) {
  struct TestCase {
    AllowedScreenCaptureLevel level;
    content::DesktopMediaID::Type type;
    bool expected;
  };
  static constexpr TestCase kCases[] = {
      {AllowedScreenCaptureLevel::kDisallowed,
       content::DesktopMediaID::TYPE_NONE, false},
      {AllowedScreenCaptureLevel::kDisallowed,
       content::DesktopMediaID::TYPE_SCREEN, false},
      {AllowedScreenCaptureLevel::kDisallowed,
       content::DesktopMediaID::TYPE_WINDOW, false},
      {AllowedScreenCaptureLevel::kDisallowed,
       content::DesktopMediaID::TYPE_WEB_CONTENTS, false},
      {AllowedScreenCaptureLevel::kSameOrigin,
       content::DesktopMediaID::TYPE_SCREEN, false},
      {AllowedScreenCaptureLevel::kSameOrigin,
       content::DesktopMediaID::TYPE_WINDOW, false},
      {AllowedScreenCaptureLevel::kSameOrigin,
       content::DesktopMediaID::TYPE_WEB_CONTENTS, true},
      {AllowedScreenCaptureLevel::kTab, content::DesktopMediaID::TYPE_SCREEN,
       false},
      {AllowedScreenCaptureLevel::kTab, content::DesktopMediaID::TYPE_WINDOW,
       false},
      {AllowedScreenCaptureLevel::kTab,
       content::DesktopMediaID::TYPE_WEB_CONTENTS, true},
      {AllowedScreenCaptureLevel::kWindow, content::DesktopMediaID::TYPE_SCREEN,
       false},
      {AllowedScreenCaptureLevel::kWindow, content::DesktopMediaID::TYPE_WINDOW,
       true},
      {AllowedScreenCaptureLevel::kWindow,
       content::DesktopMediaID::TYPE_WEB_CONTENTS, true},
      {AllowedScreenCaptureLevel::kDesktop,
       content::DesktopMediaID::TYPE_SCREEN, true},
      {AllowedScreenCaptureLevel::kDesktop,
       content::DesktopMediaID::TYPE_WINDOW, true},
      {AllowedScreenCaptureLevel::kDesktop,
       content::DesktopMediaID::TYPE_WEB_CONTENTS, true},
      // TYPE_NONE does not identify anything that could be captured, so it is
      // rejected even if capture is not restricted.
      {AllowedScreenCaptureLevel::kUnrestricted,
       content::DesktopMediaID::TYPE_NONE, false},
      {AllowedScreenCaptureLevel::kUnrestricted,
       content::DesktopMediaID::TYPE_SCREEN, true},
      {AllowedScreenCaptureLevel::kUnrestricted,
       content::DesktopMediaID::TYPE_WINDOW, true},
      {AllowedScreenCaptureLevel::kUnrestricted,
       content::DesktopMediaID::TYPE_WEB_CONTENTS, true},
  };

  for (const auto& tc : kCases) {
    EXPECT_EQ(capture_policy::IsMediaTypeAllowed(tc.level, tc.type),
              tc.expected);
  }
}

// Tests the functions that decide which tabs may be captured.
class CapturePolicyUtilsTabTest : public ChromeRenderViewHostTestHarness {
 protected:
  static constexpr char kOtherTestSite[] = "https://bar.test.org";

  const url::Origin request_origin_ = url::Origin::Create(GURL(kTestSite1));
};

TEST_F(CapturePolicyUtilsTabTest, IsTabCaptureAllowedForSameOriginTab) {
  NavigateAndCommit(GURL(kTestSite1).Resolve("/page"));

  EXPECT_FALSE(capture_policy::IsTabCaptureAllowed(
      request_origin_, AllowedScreenCaptureLevel::kDisallowed, web_contents()));
  for (AllowedScreenCaptureLevel level :
       {AllowedScreenCaptureLevel::kSameOrigin, AllowedScreenCaptureLevel::kTab,
        AllowedScreenCaptureLevel::kWindow,
        AllowedScreenCaptureLevel::kDesktop}) {
    EXPECT_TRUE(capture_policy::IsTabCaptureAllowed(request_origin_, level,
                                                    web_contents()));
    EXPECT_FALSE(
        capture_policy::IsTabCaptureAllowed(request_origin_, level, nullptr));
  }
}

TEST_F(CapturePolicyUtilsTabTest, IsTabCaptureAllowedForCrossOriginTab) {
  NavigateAndCommit(GURL(kOtherTestSite));

  EXPECT_FALSE(capture_policy::IsTabCaptureAllowed(
      request_origin_, AllowedScreenCaptureLevel::kDisallowed, web_contents()));
  EXPECT_FALSE(capture_policy::IsTabCaptureAllowed(
      request_origin_, AllowedScreenCaptureLevel::kSameOrigin, web_contents()));
  for (AllowedScreenCaptureLevel level :
       {AllowedScreenCaptureLevel::kTab, AllowedScreenCaptureLevel::kWindow,
        AllowedScreenCaptureLevel::kDesktop}) {
    EXPECT_TRUE(capture_policy::IsTabCaptureAllowed(request_origin_, level,
                                                    web_contents()));
  }
}

// The same-origin restriction applies to the origin that the document in the
// tab committed, which is not necessarily the origin of the tab's URL.
TEST_F(CapturePolicyUtilsTabTest, IsTabCaptureAllowedUsesCommittedOrigin) {
  // An error page has an opaque origin, although the URL of the tab is the URL
  // that failed to load.
  const GURL url = GURL(kTestSite1).Resolve("/page");
  ASSERT_TRUE(content::NavigationSimulator::NavigateAndFailFromBrowser(
      web_contents(), url, net::ERR_TIMED_OUT));
  ASSERT_EQ(url, web_contents()->GetLastCommittedURL());
  ASSERT_TRUE(
      web_contents()->GetPrimaryMainFrame()->GetLastCommittedOrigin().opaque());

  EXPECT_FALSE(capture_policy::IsTabCaptureAllowed(
      request_origin_, AllowedScreenCaptureLevel::kSameOrigin, web_contents()));
  EXPECT_FALSE(capture_policy::GetIncludableWebContentsFilter(
                   GURL(kTestSite1), AllowedScreenCaptureLevel::kSameOrigin)
                   .Run(web_contents()));
  EXPECT_TRUE(capture_policy::IsTabCaptureAllowed(
      request_origin_, AllowedScreenCaptureLevel::kTab, web_contents()));
}

TEST_F(CapturePolicyUtilsTabTest, IncludableWebContentsFilterFollowsPolicy) {
  NavigateAndCommit(GURL(kTestSite1));
  std::unique_ptr<content::WebContents> cross_origin_tab =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      cross_origin_tab.get(), GURL(kOtherTestSite));

  const auto filter = [](AllowedScreenCaptureLevel level) {
    return capture_policy::GetIncludableWebContentsFilter(GURL(kTestSite1),
                                                          level);
  };

  EXPECT_FALSE(
      filter(AllowedScreenCaptureLevel::kDisallowed).Run(web_contents()));
  EXPECT_FALSE(filter(AllowedScreenCaptureLevel::kDisallowed)
                   .Run(cross_origin_tab.get()));

  EXPECT_TRUE(
      filter(AllowedScreenCaptureLevel::kSameOrigin).Run(web_contents()));
  EXPECT_FALSE(filter(AllowedScreenCaptureLevel::kSameOrigin)
                   .Run(cross_origin_tab.get()));

  EXPECT_TRUE(filter(AllowedScreenCaptureLevel::kTab).Run(web_contents()));
  EXPECT_TRUE(
      filter(AllowedScreenCaptureLevel::kTab).Run(cross_origin_tab.get()));
  EXPECT_FALSE(filter(AllowedScreenCaptureLevel::kTab).Run(nullptr));
}

#if !BUILDFLAG(IS_ANDROID)
// A document picture-in-picture window is not offered as a capture source, but
// capture policy does not forbid capturing it.
TEST_F(CapturePolicyUtilsTabTest, PictureInPictureWindowIsNotIncludable) {
  NavigateAndCommit(GURL(kTestSite1));
  std::unique_ptr<content::WebContents> pip_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(pip_contents.get(),
                                                             GURL(kTestSite1));
  PictureInPictureWindowManager* const pip_window_manager =
      PictureInPictureWindowManager::GetInstance();
  pip_window_manager->EnterDocumentPictureInPicture(web_contents(),
                                                    pip_contents.get());
  ASSERT_TRUE(
      PictureInPictureWindowManager::IsChildWebContents(pip_contents.get()));

  for (AllowedScreenCaptureLevel level :
       {AllowedScreenCaptureLevel::kSameOrigin, AllowedScreenCaptureLevel::kTab,
        AllowedScreenCaptureLevel::kDesktop}) {
    EXPECT_TRUE(capture_policy::IsTabCaptureAllowed(request_origin_, level,
                                                    pip_contents.get()));
    EXPECT_FALSE(
        capture_policy::GetIncludableWebContentsFilter(GURL(kTestSite1), level)
            .Run(pip_contents.get()));
  }

  pip_window_manager->ExitPictureInPicture();
}
#endif  // !BUILDFLAG(IS_ANDROID)
