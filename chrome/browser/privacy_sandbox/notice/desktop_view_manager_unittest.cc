// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"

#include "base/test/task_environment.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager_observer.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

using notice::mojom::PrivacySandboxNotice;
using notice::mojom::PrivacySandboxNoticeEvent;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
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
    desktop_view_manager()->MaybeCreateView(
        base::BindOnce([](PrivacySandboxNotice notice) {}));
    // An observer is added once a view is created.
    desktop_view_manager()->AddObserver(observer);
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

TEST_F(DesktopViewManagerTest, OnEventOccurredWithEmptyListCrashes) {
  std::vector<PrivacySandboxNotice> required_notices;
  ON_CALL(*mock_notice_service(), GetRequiredNotices(_))
      .WillByDefault(Return(required_notices));

  EXPECT_DEATH(desktop_view_manager()->OnEventOccurred(
                   PrivacySandboxNotice::kThreeAdsApisNotice,
                   PrivacySandboxNoticeEvent::kAck),
               "");
}

TEST_F(DesktopViewManagerTest, OnEventOccurredWithShownEventDoesNotModifyList) {
  std::vector<PrivacySandboxNotice> required_notices;
  MockDesktopViewManagerObserver observer;
  EXPECT_CALL(observer, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(1);
  EXPECT_CALL(
      *mock_notice_service(),
      EventOccurred(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                                   SurfaceType::kDesktopNewTab),
                    PrivacySandboxNoticeEvent::kShown))
      .Times(1);

  // First view creation.
  CreateView(&observer);

  desktop_view_manager()->OnEventOccurred(
      PrivacySandboxNotice::kTopicsConsentNotice,
      PrivacySandboxNoticeEvent::kShown);

  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));
}

TEST_F(DesktopViewManagerTest,
       OnEventOccurredNavigatesToAllNextNoticesSuccessfully) {
  MockDesktopViewManagerObserver observer;
  EXPECT_CALL(observer,
              MaybeNavigateToNextStep(Optional(
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice)))
      .Times(1);
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(1);
  EXPECT_CALL(
      *mock_notice_service(),
      EventOccurred(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                                   SurfaceType::kDesktopNewTab),
                    PrivacySandboxNoticeEvent::kOptIn))
      .Times(1);

  // First view creation.
  CreateView(&observer);

  desktop_view_manager()->OnEventOccurred(
      PrivacySandboxNotice::kTopicsConsentNotice,
      PrivacySandboxNoticeEvent::kOptIn);

  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));
}

TEST_F(DesktopViewManagerTest, OnEventOccurredClosesAllNoticesSuccessfully) {
  MockDesktopViewManagerObserver observer;
  EXPECT_CALL(
      *mock_notice_service(),
      EventOccurred(std::make_pair(PrivacySandboxNotice::kThreeAdsApisNotice,
                                   SurfaceType::kDesktopNewTab),
                    PrivacySandboxNoticeEvent::kAck))
      .Times(1);
  EXPECT_CALL(observer, MaybeNavigateToNextStep(Eq(std::nullopt))).Times(1);

  std::vector<PrivacySandboxNotice> required_notices = {
      PrivacySandboxNotice::kThreeAdsApisNotice};
  ON_CALL(*mock_notice_service(), GetRequiredNotices(_))
      .WillByDefault(Return(required_notices));
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(1);

  // First view creation.
  CreateView(&observer);
  desktop_view_manager()->OnEventOccurred(
      PrivacySandboxNotice::kThreeAdsApisNotice,
      PrivacySandboxNoticeEvent::kAck);

  EXPECT_TRUE(desktop_view_manager()->GetPendingNoticesToShow().empty());
}

}  // namespace privacy_sandbox
