// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/capture_policy_utils.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
  base::Value matchlist(base::Value::Type::LIST);
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->Set(prefs::kScreenCaptureAllowedByOrigins, matchlist);
  EXPECT_EQ(AllowedScreenCaptureLevel::kUnrestricted,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Window/Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideWindowTabs) {
  base::Value matchlist(base::Value::Type::LIST);
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->Set(prefs::kWindowCaptureAllowedByOrigins, matchlist);
  EXPECT_EQ(AllowedScreenCaptureLevel::kWindow,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideTabs) {
  base::Value matchlist(base::Value::Type::LIST);
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->Set(prefs::kTabCaptureAllowedByOrigins, matchlist);
  EXPECT_EQ(AllowedScreenCaptureLevel::kTab,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that the Same Origin Tab allowed list overrides |kScreenCaptureAllowed|.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideSameOriginTabs) {
  base::Value matchlist(base::Value::Type::LIST);
  matchlist.Append(kTestSite1Pattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->Set(prefs::kSameOriginTabCaptureAllowedByOrigins, matchlist);
  EXPECT_EQ(AllowedScreenCaptureLevel::kSameOrigin,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Test that an item that doesn't match any list still respects the default.
TEST_F(CapturePolicyUtilsTest, SimpleOverrideNoMatches) {
  base::Value matchlist(base::Value::Type::LIST);
  matchlist.Append(kTestSite1NonMatchingPattern);
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->Set(prefs::kSameOriginTabCaptureAllowedByOrigins, matchlist);
  EXPECT_EQ(AllowedScreenCaptureLevel::kDisallowed,
            capture_policy::GetAllowedCaptureLevel(GURL(kTestSite1), *prefs()));
}

// Ensure that a full wildcard policy is accepted.
TEST_F(CapturePolicyUtilsTest, TestWildcard) {
  base::Value matchlist(base::Value::Type::LIST);
  matchlist.Append("*");
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->Set(prefs::kTabCaptureAllowedByOrigins, matchlist);
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
  base::Value fullCaptureList(base::Value::Type::LIST);
  fullCaptureList.Append("a.com");
  fullCaptureList.Append("b.com");
  fullCaptureList.Append("c.com");
  prefs()->SetBoolean(prefs::kScreenCaptureAllowed, false);
  prefs()->Set(prefs::kScreenCaptureAllowedByOrigins, fullCaptureList);

  base::Value windowTabList(base::Value::Type::LIST);
  windowTabList.Append("b.com");
  prefs()->Set(prefs::kWindowCaptureAllowedByOrigins, windowTabList);

  base::Value tabList(base::Value::Type::LIST);
  tabList.Append("c.com");
  tabList.Append("d.com");
  prefs()->Set(prefs::kTabCaptureAllowedByOrigins, tabList);

  base::Value sameOriginTabList(base::Value::Type::LIST);
  sameOriginTabList.Append("d.com");
  prefs()->Set(prefs::kSameOriginTabCaptureAllowedByOrigins, sameOriginTabList);
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

  base::Value sameOriginTabList(base::Value::Type::LIST);
  sameOriginTabList.Append("github.io");
  prefs()->Set(prefs::kSameOriginTabCaptureAllowedByOrigins, sameOriginTabList);

  base::Value tabList(base::Value::Type::LIST);
  tabList.Append("foo.github.io");
  prefs()->Set(prefs::kTabCaptureAllowedByOrigins, tabList);

  base::Value fullCaptureList(base::Value::Type::LIST);
  fullCaptureList.Append("[*.]github.io");
  prefs()->Set(prefs::kScreenCaptureAllowedByOrigins, fullCaptureList);

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
