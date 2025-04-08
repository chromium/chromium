// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"

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
BASE_FEATURE(kTestFeatureB, "TestFeatureB", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeatureC, "TestFeatureC", base::FEATURE_DISABLED_BY_DEFAULT);

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
  NoticeCatalog catalog_;
};

TEST_F(PrivacySandboxNoticeCatalogTest, InitialState) {
  EXPECT_THAT(catalog_.GetNoticeApis(), IsEmpty());
  EXPECT_THAT(catalog_.GetNoticeMap(), IsEmpty());
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

TEST_F(PrivacySandboxNoticeCatalogTest, RegisterNoticeGroup) {
  NoticeApi* target_api1 = catalog_.RegisterAndRetrieveNewApi();
  NoticeApi* target_api2 = catalog_.RegisterAndRetrieveNewApi();
  NoticeApi* prereq_api = catalog_.RegisterAndRetrieveNewApi();

  const NoticeId consent_id_desktop = {
      PrivacySandboxNotice::kTopicsConsentNotice, kDesktopNewTab};
  const NoticeId consent_id_clank = {PrivacySandboxNotice::kTopicsConsentNotice,
                                     kClankBrApp};
  const NoticeId notice_id_desktop = {
      PrivacySandboxNotice::kProtectedAudienceMeasurementNotice,
      kDesktopNewTab};

  catalog_.RegisterNoticeGroup(&Make<Consent>,
                               {{consent_id_desktop, &kTestFeatureA},
                                {consent_id_clank, &kTestFeatureB}},
                               {target_api1});

  catalog_.RegisterNoticeGroup(&Make<Notice>,
                               {{notice_id_desktop, &kTestFeatureC}},
                               {target_api1, target_api2}, {prereq_api});

  EXPECT_EQ(catalog_.GetNoticeApis().size(), 3u);
  EXPECT_EQ(catalog_.GetNoticeMap().size(), 3u);

  ASSERT_TRUE(catalog_.GetNoticeMap().contains(consent_id_desktop));
  Notice* consent_desktop =
      catalog_.GetNoticeMap().at(consent_id_desktop).get();
  EXPECT_EQ(consent_desktop->GetNoticeType(), kConsent);
  EXPECT_EQ(consent_desktop->GetFeature(), &kTestFeatureA);
  EXPECT_THAT(consent_desktop->GetTargetApis(), ElementsAre(target_api1));
  EXPECT_THAT(consent_desktop->GetPreReqApis(), IsEmpty());
  EXPECT_THAT(target_api1->GetLinkedNotices(), Contains(consent_desktop));

  ASSERT_TRUE(catalog_.GetNoticeMap().contains(consent_id_clank));
  Notice* consent_clank = catalog_.GetNoticeMap().at(consent_id_clank).get();
  EXPECT_EQ(consent_clank->GetNoticeType(), kConsent);
  EXPECT_EQ(consent_clank->GetFeature(), &kTestFeatureB);
  EXPECT_THAT(consent_clank->GetTargetApis(), ElementsAre(target_api1));
  EXPECT_THAT(consent_clank->GetPreReqApis(), IsEmpty());
  EXPECT_THAT(target_api1->GetLinkedNotices(), Contains(consent_clank));

  ASSERT_TRUE(catalog_.GetNoticeMap().contains(notice_id_desktop));
  Notice* notice_desktop = catalog_.GetNoticeMap().at(notice_id_desktop).get();
  EXPECT_EQ(notice_desktop->GetNoticeType(), kNotice);
  EXPECT_EQ(notice_desktop->GetFeature(), &kTestFeatureC);
  EXPECT_THAT(notice_desktop->GetTargetApis(),
              ElementsAre(target_api1, target_api2));
  EXPECT_THAT(notice_desktop->GetPreReqApis(), ElementsAre(prereq_api));
  EXPECT_THAT(target_api1->GetLinkedNotices(), Contains(notice_desktop));
  EXPECT_THAT(target_api2->GetLinkedNotices(), Contains(notice_desktop));
}

}  // namespace
}  // namespace privacy_sandbox
