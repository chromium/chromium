// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"

#include <set>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using testing::Contains;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Not;
using testing::Truly;

using enum privacy_sandbox::NoticeType;
using enum privacy_sandbox::SurfaceType;
using enum privacy_sandbox::notice::mojom::PrivacySandboxNotice;

// TODO(crbug.com/392612108): Add a test library util class that implements
// these, so these can be reused with browsertests later.
class PrivacySandboxNoticeCatalogTest : public testing::Test {
 protected:
  NoticeCatalogImpl catalog_;
};

// Test that Populate actually registers APIs and Notices.
TEST_F(PrivacySandboxNoticeCatalogTest, PopulatesCatalog) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));
}

// No duplicate Notices (same pair of Name and surface).
TEST_F(PrivacySandboxNoticeCatalogTest, NoDuplicateNoticeIds) {
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));

  std::set<Notice*> notice_pointers;
  for (Notice* notice : catalog_.GetNotices()) {
    ASSERT_NE(notice, nullptr);
    EXPECT_TRUE(notice_pointers.insert(notice).second);
  }
  EXPECT_EQ(notice_pointers.size(), catalog_.GetNotices().size());
}

// All notices must point to a unique Feature.
TEST_F(PrivacySandboxNoticeCatalogTest, UniqueFeaturesPerNoticeInstance) {
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));

  std::set<const base::Feature*> features_seen;
  for (const Notice* notice : catalog_.GetNotices()) {
    ASSERT_NE(notice, nullptr);
    const base::Feature* feature = notice->feature();
    ASSERT_NE(feature, nullptr);
    EXPECT_TRUE(features_seen.insert(feature).second);
  }
}

// All notices have a unique storage name.
TEST_F(PrivacySandboxNoticeCatalogTest, UniqueStorageNamePerNoticeInstance) {
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));

  std::set<std::string> storage_names;
  for (const Notice* notice : catalog_.GetNotices()) {
    ASSERT_NE(notice, nullptr);
    EXPECT_TRUE(storage_names.insert(notice->GetStorageName()).second);
  }
}

// All notices must map to at least one target API.
TEST_F(PrivacySandboxNoticeCatalogTest, AllNoticesTargetAtLeastOneApi) {
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));

  for (Notice* notice : catalog_.GetNotices()) {
    ASSERT_NE(notice, nullptr);
    EXPECT_THAT(notice->target_apis(), Not(IsEmpty()));
  }
}

// Groups are unique per Surface Type when set.
TEST_F(PrivacySandboxNoticeCatalogTest, UniqueViewGroupPerSurfaceType) {
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));

  std::map<SurfaceType, std::set<std::pair<NoticeViewGroup, int>>>
      view_groups_per_surface;

  for (const Notice* notice : catalog_.GetNotices()) {
    ASSERT_NE(notice, nullptr);
    auto [group, order] = notice->view_group();
    if (group == NoticeViewGroup::kNotSet) {
      continue;
    }

    auto& view_groups_for_surface =
        view_groups_per_surface[notice->notice_id().second];

    EXPECT_TRUE(view_groups_for_surface.insert({group, order}).second)
        << "Duplicate view group (" << static_cast<int>(group) << ", " << order
        << ") for surface type "
        << static_cast<int>(notice->notice_id().second);
  }
}

// TODO(boujane) Add a test to ensure notices in the same group
// don't have duplicate targets.

// All APIs must be covered by at least one Notice.
TEST_F(PrivacySandboxNoticeCatalogTest, AllApisAreTargetedByAtLeastOneNotice) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));

  for (const auto& api : catalog_.GetNoticeApis()) {
    ASSERT_NE(api, nullptr);
    EXPECT_THAT(api->linked_notices(), Not(IsEmpty()));
  }
}

// All registered APIs must have unique pointers.
TEST_F(PrivacySandboxNoticeCatalogTest, UniqueApiInstances) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));

  std::set<NoticeApi*> api_pointers;
  for (const auto& api_ptr : catalog_.GetNoticeApis()) {
    ASSERT_NE(api_ptr, nullptr);
    EXPECT_TRUE(api_pointers.insert(api_ptr).second);
  }
  EXPECT_EQ(api_pointers.size(), catalog_.GetNoticeApis().size());
}

// All APIs listed as Targets for any Notice must be present in the main list of
// registered APIs.
TEST_F(PrivacySandboxNoticeCatalogTest, TargetApisAreValid) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));

  std::set<const NoticeApi*> valid_api_pointers;
  for (const auto& api : catalog_.GetNoticeApis()) {
    ASSERT_NE(api, nullptr);
    valid_api_pointers.insert(api);
  }

  EXPECT_THAT(valid_api_pointers, Not(IsEmpty()));

  for (Notice* notice : catalog_.GetNotices()) {
    ASSERT_NE(notice, nullptr);
    for (const NoticeApi* target_api : notice->target_apis()) {
      EXPECT_THAT(valid_api_pointers, Contains(target_api));
    }
  }
}

// All pre-requisite APIs listed for any Notice must be present in the main list
// of registered APIs.
TEST_F(PrivacySandboxNoticeCatalogTest, PrerequisiteApisAreValid) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));
  EXPECT_THAT(catalog_.GetNotices(), Not(IsEmpty()));

  // Create a set of valid API pointers for quick lookup.
  std::set<const NoticeApi*> valid_api_pointers;
  for (const auto& api : catalog_.GetNoticeApis()) {
    ASSERT_NE(api, nullptr);
    valid_api_pointers.insert(api);
  }

  EXPECT_THAT(valid_api_pointers, Not(IsEmpty()));

  for (Notice* notice : catalog_.GetNotices()) {
    ASSERT_NE(notice, nullptr);
    for (const NoticeApi* prereq_api : notice->pre_req_apis()) {
      EXPECT_THAT(valid_api_pointers, Contains(prereq_api));
    }
  }
}

TEST_F(PrivacySandboxNoticeCatalogTest,
       GetNotice_ReturnsNoticeWhenExistsAndIsInList) {
  NoticeId id = {kTopicsConsentNotice, kDesktopNewTab};
  Notice* notice_from_get_notice = catalog_.GetNotice(id);

  // Verify the notice was found.
  ASSERT_NE(notice_from_get_notice, nullptr);
  EXPECT_EQ(notice_from_get_notice->notice_id(), id);

  // Verify that GetNotice returns an object that's also in GetNotices.
  EXPECT_THAT(catalog_.GetNotices(), Contains(notice_from_get_notice));
}

TEST_F(PrivacySandboxNoticeCatalogTest, GetNotice_ReturnsNullptrWhenNotExists) {
  NoticeId not_found_id = {static_cast<PrivacySandboxNotice>(999),
                           SurfaceType::kDesktopNewTab};

  EXPECT_EQ(catalog_.GetNotice(not_found_id), nullptr);
}

class PrivacySandboxNoticeCatalogPopulateAllNoticesTest
    : public PrivacySandboxNoticeCatalogTest,
      public testing::WithParamInterface<int> {};

TEST_P(PrivacySandboxNoticeCatalogPopulateAllNoticesTest,
       AllNoticeEnumsExistsInTheNoticesList) {
  PrivacySandboxNotice notice_enum_to_find =
      static_cast<PrivacySandboxNotice>(GetParam());

  EXPECT_THAT(catalog_.GetNotices(),
              Contains(Truly([notice_enum_to_find](Notice* notice) {
                return notice &&
                       notice->notice_id().first == notice_enum_to_find;
              })))
      << "Notice enum value " << notice_enum_to_find
      << " was not found in the NoticeCatalog's list.";
}

INSTANTIATE_TEST_SUITE_P(
    PrivacySandboxNoticeCatalogPopulateAllNoticesTest,
    PrivacySandboxNoticeCatalogPopulateAllNoticesTest,
    testing::Range(static_cast<int>(PrivacySandboxNotice::kMinValue),
                   static_cast<int>(PrivacySandboxNotice::kMaxValue) + 1));

}  // namespace
}  // namespace privacy_sandbox
