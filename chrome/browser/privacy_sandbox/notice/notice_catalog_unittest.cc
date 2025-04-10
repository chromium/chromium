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

using enum privacy_sandbox::NoticeType;
using enum privacy_sandbox::SurfaceType;

BASE_FEATURE(kTestFeatureA, "TestFeatureA", base::FEATURE_DISABLED_BY_DEFAULT);

// Test Consent ID, with arbitrary NoticeType and SurfaceType.
constexpr NoticeId kTestConsentId = {PrivacySandboxNotice::kTopicsConsentNotice,
                                     kClankBrApp};

// Helper function to create Notice/Consent objects easily in tests
template <typename T>
std::unique_ptr<Notice> Make(NoticeId notice_id) {
  return std::make_unique<T>(notice_id);
}

class PrivacySandboxNoticeCatalogTest : public testing::Test {
 protected:
  NoticeCatalogImpl catalog_;
};

TEST_F(PrivacySandboxNoticeCatalogTest, InitialState) {
  EXPECT_THAT(catalog_.GetNoticeApis(), IsEmpty());
  EXPECT_THAT(catalog_.GetNoticeMap(), IsEmpty());
  EXPECT_FALSE(catalog_.IsPopulated());
}

TEST_F(PrivacySandboxNoticeCatalogTest, IsPopulated) {
  EXPECT_FALSE(catalog_.IsPopulated());
  catalog_.Populate();
  EXPECT_TRUE(catalog_.IsPopulated());
  EXPECT_DEATH(catalog_.Populate(), "");
}

TEST_F(PrivacySandboxNoticeCatalogTest, RegisterAndRetrieveNewApi) {
  NoticeApi* api1 = catalog_.RegisterAndRetrieveNewApi();
  EXPECT_NE(api1, nullptr);
  EXPECT_EQ(catalog_.GetNoticeApis().size(), 1u);
  EXPECT_EQ(catalog_.GetNoticeApis()[0].get(), api1);

  NoticeApi* api2 = catalog_.RegisterAndRetrieveNewApi();
  EXPECT_NE(api2, nullptr);
  EXPECT_NE(api1, api2);
  EXPECT_EQ(catalog_.GetNoticeApis().size(), 2u);
  EXPECT_EQ(catalog_.GetNoticeApis()[1].get(), api2);
}

TEST_F(PrivacySandboxNoticeCatalogTest, RegisterAndRetrieveNewNotice) {
  NoticeApi* target_api = catalog_.RegisterAndRetrieveNewApi();
  NoticeApi* prereq_api = catalog_.RegisterAndRetrieveNewApi();

  Notice* consent_notice =
      catalog_.RegisterAndRetrieveNewNotice(&Make<Consent>, kTestConsentId);

  ASSERT_NE(consent_notice, nullptr);
  EXPECT_EQ(consent_notice->GetNoticeId(), kTestConsentId);
  EXPECT_EQ(consent_notice->GetNoticeType(), kConsent);
  EXPECT_EQ(catalog_.GetNoticeMap().size(), 1u);
  ASSERT_TRUE(catalog_.GetNoticeMap().contains(kTestConsentId));
  EXPECT_EQ(catalog_.GetNoticeMap().at(kTestConsentId).get(), consent_notice);

  consent_notice->SetFeature(&kTestFeatureA)
      ->SetTargetApis({target_api})
      ->SetPreReqApis({prereq_api});

  EXPECT_EQ(consent_notice->GetFeature(), &kTestFeatureA);
  EXPECT_THAT(consent_notice->GetTargetApis(), ElementsAre(target_api));
  EXPECT_THAT(consent_notice->GetPreReqApis(), ElementsAre(prereq_api));
  EXPECT_THAT(target_api->GetLinkedNotices(), Contains(consent_notice));
}

// TODO(crbug.com/392612108): Add a test library util class that implements
// these, so these can be reused with browsertests later.
class PrivacySandboxNoticeCatalogPopulateTest : public testing::Test {
 protected:
  void SetUp() override { catalog_.Populate(); }

  NoticeCatalogImpl catalog_;
};

// Test that Populate actually registers APIs and Notices.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest, PopulatesCatalog) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));
  EXPECT_THAT(catalog_.GetNoticeMap(), Not(IsEmpty()));
}

// No duplicate Notices (same pair of Name and surface).
TEST_F(PrivacySandboxNoticeCatalogPopulateTest, NoDuplicateNoticeIds) {
  EXPECT_THAT(catalog_.GetNoticeMap(), Not(IsEmpty()));

  std::set<Notice*> notice_pointers;
  for (const auto& [_, notice] : catalog_.GetNoticeMap()) {
    ASSERT_NE(notice.get(), nullptr);
    EXPECT_TRUE(notice_pointers.insert(notice.get()).second);
  }
  EXPECT_EQ(notice_pointers.size(), catalog_.GetNoticeMap().size());
}

// All notices must point to a unique Feature.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest,
       UniqueFeaturesPerNoticeInstance) {
  EXPECT_THAT(catalog_.GetNoticeMap(), Not(IsEmpty()));

  std::set<const base::Feature*> features_seen;
  for (const auto& [_, notice] : catalog_.GetNoticeMap()) {
    ASSERT_NE(notice.get(), nullptr);
    const base::Feature* feature = notice->GetFeature();
    ASSERT_NE(feature, nullptr);
    EXPECT_TRUE(features_seen.insert(feature).second);
  }
}

// All notices have a unique storage name.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest,
       UniqueStorageNamePerNoticeInstance) {
  EXPECT_THAT(catalog_.GetNoticeMap(), Not(IsEmpty()));

  std::set<std::string> storage_names;
  for (const auto& [_, notice] : catalog_.GetNoticeMap()) {
    ASSERT_NE(notice.get(), nullptr);
    EXPECT_TRUE(storage_names.insert(notice->GetStorageName()).second);
  }
}

// All notices must map to at least one target API.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest, AllNoticesTargetAtLeastOneApi) {
  EXPECT_THAT(catalog_.GetNoticeMap(), Not(IsEmpty()));

  for (const auto& [notice_id, notice] : catalog_.GetNoticeMap()) {
    ASSERT_NE(notice.get(), nullptr);
    EXPECT_THAT(notice->GetTargetApis(), Not(IsEmpty()));
  }
}

// All APIs must be covered by at least one Notice.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest,
       AllApisAreTargetedByAtLeastOneNotice) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));

  for (const auto& api : catalog_.GetNoticeApis()) {
    ASSERT_NE(api.get(), nullptr);
    EXPECT_THAT(api->GetLinkedNotices(), Not(IsEmpty()));
  }
}

// All registered APIs must have unique pointers.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest, UniqueApiInstances) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));

  std::set<NoticeApi*> api_pointers;
  for (const auto& api_ptr : catalog_.GetNoticeApis()) {
    ASSERT_NE(api_ptr.get(), nullptr);
    EXPECT_TRUE(api_pointers.insert(api_ptr.get()).second);
  }
  EXPECT_EQ(api_pointers.size(), catalog_.GetNoticeApis().size());
}

// All APIs listed as Targets for any Notice must be present in the main list of
// registered APIs.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest, TargetApisAreValid) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));
  EXPECT_THAT(catalog_.GetNoticeMap(), Not(IsEmpty()));

  std::set<const NoticeApi*> valid_api_pointers;
  for (const auto& api : catalog_.GetNoticeApis()) {
    ASSERT_NE(api.get(), nullptr);
    valid_api_pointers.insert(api.get());
  }

  EXPECT_THAT(valid_api_pointers, Not(IsEmpty()));

  for (const auto& [notice_id, notice] : catalog_.GetNoticeMap()) {
    ASSERT_NE(notice.get(), nullptr);
    for (const NoticeApi* target_api : notice->GetTargetApis()) {
      EXPECT_THAT(valid_api_pointers, Contains(target_api));
    }
  }
}

// All pre-requisite APIs listed for any Notice must be present in the main list
// of registered APIs.
TEST_F(PrivacySandboxNoticeCatalogPopulateTest, PrerequisiteApisAreValid) {
  EXPECT_THAT(catalog_.GetNoticeApis(), Not(IsEmpty()));
  EXPECT_THAT(catalog_.GetNoticeMap(), Not(IsEmpty()));

  // Create a set of valid API pointers for quick lookup.
  std::set<const NoticeApi*> valid_api_pointers;
  for (const auto& api : catalog_.GetNoticeApis()) {
    ASSERT_NE(api.get(), nullptr);
    valid_api_pointers.insert(api.get());
  }

  EXPECT_THAT(valid_api_pointers, Not(IsEmpty()));

  for (const auto& [notice_id, notice] : catalog_.GetNoticeMap()) {
    ASSERT_NE(notice.get(), nullptr);
    for (const NoticeApi* prereq_api : notice->GetPreReqApis()) {
      EXPECT_THAT(valid_api_pointers, Contains(prereq_api));
    }
  }
}

class PrivacySandboxNoticeCatalogPopulateAllNoticesTest
    : public PrivacySandboxNoticeCatalogPopulateTest,
      public testing::WithParamInterface<int> {};

TEST_P(PrivacySandboxNoticeCatalogPopulateAllNoticesTest,
       AllNoticeEnumsExistsInTheNoticeMap) {
  PrivacySandboxNotice notice_enum_to_find =
      static_cast<PrivacySandboxNotice>(GetParam());

  bool found = false;
  for (const auto& [notice_id, notice_ptr] : catalog_.GetNoticeMap()) {
    if (notice_id.first != notice_enum_to_find) {
      continue;
    }
    found = true;
    EXPECT_NE(notice_ptr, nullptr);
    break;
  }

  EXPECT_TRUE(found) << "Notice enum value " << notice_enum_to_find
                     << " was not found in the NoticeCatalog map.";
}

INSTANTIATE_TEST_SUITE_P(
  PrivacySandboxNoticeCatalogPopulateAllNoticesTest,
    PrivacySandboxNoticeCatalogPopulateAllNoticesTest,
    testing::Range(static_cast<int>(PrivacySandboxNotice::kMinValue),
                   static_cast<int>(PrivacySandboxNotice::kMaxValue) + 1));

}  // namespace
}  // namespace privacy_sandbox
