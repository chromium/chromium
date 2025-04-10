// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/notice_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_storage.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_catalog.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using ::privacy_sandbox::notice::mojom::PrivacySandboxNotice;
using ::privacy_sandbox::notice::mojom::PrivacySandboxNoticeEvent;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::StrEq;
using ::testing::Test;
using Event = PrivacySandboxNoticeEvent;
using enum PrivacySandboxNotice;
using enum SurfaceType;

BASE_FEATURE(kTestFeatureA, "TestFeatureA", base::FEATURE_DISABLED_BY_DEFAULT);

std::unique_ptr<Notice> MakeNotice(NoticeId id) {
  return std::make_unique<Notice>(id);
}

Notice* RegisterTestNotice(NoticeCatalog* catalog, NoticeId notice_id) {
  CHECK(catalog);
  Notice* registered_notice =
      catalog->RegisterAndRetrieveNewNotice(&MakeNotice, notice_id)
          ->SetFeature(&kTestFeatureA);
  CHECK(registered_notice);
  return registered_notice;
}

class PrivacySandboxNoticeServiceTest : public Test {
 public:
  PrivacySandboxNoticeServiceTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    auto storage = std::make_unique<MockNoticeStorage>();
    mock_storage_ = storage.get();
    auto catalog = std::make_unique<NoticeCatalogImpl>();
    catalog_ptr_ = catalog.get();

    notice_service_ = std::make_unique<PrivacySandboxNoticeService>(
        profile_.get(), std::move(catalog), std::move(storage));
  }

 protected:
  PrivacySandboxNoticeService* notice_service() {
    return notice_service_.get();
  }
  MockNoticeStorage* mock_storage() { return mock_storage_; }
  TestingProfile* profile() { return profile_.get(); }
  NoticeCatalog* catalog() { return catalog_ptr_; }
  content::BrowserTaskEnvironment& task_environment() {
    return browser_task_environment_;
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<PrivacySandboxNoticeService> notice_service_;
  raw_ptr<MockNoticeStorage> mock_storage_ = nullptr;
  raw_ptr<NoticeCatalog> catalog_ptr_ = nullptr;
};

TEST_F(PrivacySandboxNoticeServiceTest,
       EventOccurred_NoticeFound_CallsRecordEvent) {
  const NoticeId notice_id{kThreeAdsApisNotice, kDesktopNewTab};
  const Event event = Event::kAck;

  Notice* test_notice = RegisterTestNotice(catalog(), notice_id);
  std::string expected_notice_name = test_notice->GetStorageName();
  PrefService* expected_prefs = profile()->GetPrefs();
  base::Time expected_time = base::Time::Now();

  EXPECT_CALL(*mock_storage(),
              RecordEvent(Eq(expected_prefs), StrEq(expected_notice_name),
                          Eq(event), Eq(expected_time)))
      .Times(1);

  notice_service()->EventOccurred(notice_id, event);
}

TEST_F(PrivacySandboxNoticeServiceTest, EventOccurred_NoticeNotFound_Crashes) {
  NoticeId unregistered_notice_id{kTopicsConsentNotice, kDesktopNewTab};

  EXPECT_DEATH(
      notice_service()->EventOccurred(unregistered_notice_id, Event::kShown),
      "");
}

TEST_F(PrivacySandboxNoticeServiceTest,
       GetRequiredNotices_HandlesEmptyCatalog) {
  EXPECT_THAT(notice_service()->GetRequiredNotices(kDesktopNewTab), IsEmpty());
  EXPECT_THAT(notice_service()->GetRequiredNotices(kClankBrApp), IsEmpty());
  EXPECT_THAT(notice_service()->GetRequiredNotices(kClankCustomTab), IsEmpty());
  // TODO(crbug.com/392612108): Write tests when GetRequiredNotices is
  // implemented.
}

}  // namespace
}  // namespace privacy_sandbox
