// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"

#include "base/test/task_environment.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager_observer.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

using notice::mojom::PrivacySandboxNotice;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Return;

class DesktopViewManagerTest : public testing::Test {
 public:
  DesktopViewManagerTest()
      : browser_task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    // Mocking notice service.
    std::vector<PrivacySandboxNotice> required_notices = {
        PrivacySandboxNotice::kTopicsConsentNotice,
        PrivacySandboxNotice::kProtectedAudienceMeasurementNotice};
    mock_notice_service_ = std::make_unique<MockPrivacySandboxNoticeService>();
    ON_CALL(*mock_notice_service_, GetRequiredNotices(_))
        .WillByDefault(Return(required_notices));

    // Desktop view manager creation.
    desktop_view_manager_ =
        std::make_unique<DesktopViewManager>(mock_notice_service_.get());
  }

  void TearDown() override {
    desktop_view_manager_.reset();
    mock_notice_service_.reset();
  }

  void CreateView(MockDesktopViewManagerObserver* observer) {
    desktop_view_manager_->MaybeCreateView();
    // Once a view is created, an observer is added.
    desktop_view_manager_->AddObserver(observer);
  }

  DesktopViewManager* desktop_view_manager() {
    return desktop_view_manager_.get();
  }

  MockPrivacySandboxNoticeService* mock_notice_service() {
    return mock_notice_service_.get();
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<DesktopViewManager> desktop_view_manager_;
  std::unique_ptr<MockPrivacySandboxNoticeService> mock_notice_service_;
};

TEST_F(DesktopViewManagerTest, MaybeCreateViewDoesNotNotifyOnSameList) {
  MockDesktopViewManagerObserver observer1;
  MockDesktopViewManagerObserver observer2;
  EXPECT_CALL(observer1, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(observer2, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(2);

  // First view creation.
  CreateView(&observer1);
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));

  // A second view is created on the same list.
  CreateView(&observer2);
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));
}

TEST_F(DesktopViewManagerTest,
       MaybeCreateViewNotifiesToCloseAllSuccessfullyOnChangedList) {
  MockDesktopViewManagerObserver observer1;
  MockDesktopViewManagerObserver observer2;
  EXPECT_CALL(observer1, MaybeNavigateToNextStep(Eq(std::nullopt))).Times(1);
  // Observer2 is created after the new list, no need to be notified.
  EXPECT_CALL(observer2, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(2);

  // First view creation.
  CreateView(&observer1);
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));

  // Update required notice list.
  std::vector<PrivacySandboxNotice> required_notices = {
      PrivacySandboxNotice::kThreeAdsApisNotice};
  ON_CALL(*mock_notice_service(), GetRequiredNotices(_))
      .WillByDefault(Return(required_notices));

  // Observers should be notified and pending notices list should be updated.
  CreateView(&observer2);
  EXPECT_THAT(desktop_view_manager()->GetPendingNoticesToShow(),
              ElementsAre(PrivacySandboxNotice::kThreeAdsApisNotice));
}

}  // namespace privacy_sandbox
