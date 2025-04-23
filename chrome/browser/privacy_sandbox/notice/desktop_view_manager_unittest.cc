// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager_observer.h"
#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_model.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {

// Allow tests to access private variables and functions from
// `DesktopViewManager`.
class DesktopViewManagerTestPeer {
 public:
  explicit DesktopViewManagerTestPeer(DesktopViewManager* desktop_view_manager)
      : desktop_view_manager_(desktop_view_manager) {}
  ~DesktopViewManagerTestPeer() = default;

  void SetPendingNotices(
      std::vector<notice::mojom::PrivacySandboxNotice> pending_notices) {
    desktop_view_manager_->SetPendingNoticesToShow(pending_notices);
  }

  void CreateView(BrowserWindowInterface* browser,
                  DesktopViewManager::ShowViewCallback show) {
    desktop_view_manager_->MaybeCreateView(browser, std::move(show));
  }

 private:
  raw_ptr<DesktopViewManager> desktop_view_manager_;
};

namespace {
using base::MockCallback;
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
    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    mock_notice_service_ = std::make_unique<MockPrivacySandboxNoticeService>();
    desktop_view_manager_ =
        std::make_unique<DesktopViewManager>(mock_notice_service_.get());
    desktop_view_manager_test_peer_ =
        std::make_unique<DesktopViewManagerTestPeer>(
            desktop_view_manager_.get());
  }

  void TearDown() override {
    desktop_view_manager_test_peer_.reset();
    desktop_view_manager_.reset();
    mock_notice_service_.reset();
  }

  void SetRequiredNotices(std::vector<PrivacySandboxNotice> required_notices) {
    ON_CALL(*mock_notice_service_, GetRequiredNotices(_))
        .WillByDefault(Return(required_notices));
  }

  void CreateView(MockDesktopViewManagerObserver* observer,
                  DesktopViewManager::ShowViewCallback callback) {
    desktop_view_manager_test_peer_->CreateView(browser_window_interface_.get(),
                                                std::move(callback));
    // An observer is added once a view is created.
    desktop_view_manager()->AddObserver(observer);
  }

  void SetPendingNotices(std::vector<PrivacySandboxNotice> pending_notices) {
    desktop_view_manager_test_peer_->SetPendingNotices(pending_notices);
  }

  DesktopViewManager* desktop_view_manager() {
    return desktop_view_manager_.get();
  }

  MockPrivacySandboxNoticeService* mock_notice_service() {
    return mock_notice_service_.get();
  }

 private:
  std::unique_ptr<DesktopViewManagerTestPeer> desktop_view_manager_test_peer_;
  content::BrowserTaskEnvironment browser_task_environment_;
  std::unique_ptr<DesktopViewManager> desktop_view_manager_;
  std::unique_ptr<MockPrivacySandboxNoticeService> mock_notice_service_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
};

TEST_F(DesktopViewManagerTest, MaybeCreateView_EmptyListDoesNotRun) {
  SetRequiredNotices({});

  // Expectations.
  MockDesktopViewManagerObserver observer;
  EXPECT_CALL(observer, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(1);
  MockCallback<DesktopViewManager::ShowViewCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);

  // First view creation.
  CreateView(&observer, callback.Get());
  EXPECT_TRUE(desktop_view_manager()->GetPendingNoticesToShow().empty());
}

TEST_F(DesktopViewManagerTest, MaybeCreateView_SameListDoesNotNotify) {
  SetRequiredNotices(
      {PrivacySandboxNotice::kTopicsConsentNotice,
       PrivacySandboxNotice::kProtectedAudienceMeasurementNotice});

  // Expectations.
  MockDesktopViewManagerObserver observer1, observer2;
  EXPECT_CALL(observer1, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(observer2, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(2);
  MockCallback<DesktopViewManager::ShowViewCallback> callback1, callback2;
  EXPECT_CALL(callback1, Run);
  EXPECT_CALL(callback2, Run);

  // First view creation.
  CreateView(&observer1, callback1.Get());
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));

  // A second view is created on the same list.
  CreateView(&observer2, callback2.Get());
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));
}

TEST_F(DesktopViewManagerTest, MaybeCreateView_ChangedListNotifiesToCloseAll) {
  SetRequiredNotices(
      {PrivacySandboxNotice::kTopicsConsentNotice,
       PrivacySandboxNotice::kProtectedAudienceMeasurementNotice});

  // Expectations.
  MockDesktopViewManagerObserver observer1, observer2;
  EXPECT_CALL(observer1, MaybeNavigateToNextStep(Eq(std::nullopt))).Times(1);
  // Observer2 is created after the new list, no need to be notified.
  EXPECT_CALL(observer2, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(*mock_notice_service(), GetRequiredNotices(_)).Times(2);
  MockCallback<DesktopViewManager::ShowViewCallback> callback1, callback2;
  EXPECT_CALL(callback1, Run);
  EXPECT_CALL(callback2, Run);

  // First view creation.
  CreateView(&observer1, callback1.Get());
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));

  // Update required notice list.
  SetRequiredNotices({PrivacySandboxNotice::kThreeAdsApisNotice});
  CreateView(&observer2, callback2.Get());
  EXPECT_THAT(desktop_view_manager()->GetPendingNoticesToShow(),
              ElementsAre(PrivacySandboxNotice::kThreeAdsApisNotice));
}

TEST_F(DesktopViewManagerTest, OnEventOccurred_EmptyListCrashes) {
  SetPendingNotices({});
  EXPECT_DEATH(desktop_view_manager()->OnEventOccurred(
                   PrivacySandboxNotice::kThreeAdsApisNotice,
                   PrivacySandboxNoticeEvent::kAck),
               "");
}

TEST_F(DesktopViewManagerTest, OnEventOccurred_ShownEventDoesNotModifyList) {
  SetPendingNotices(
      {PrivacySandboxNotice::kTopicsConsentNotice,
       PrivacySandboxNotice::kProtectedAudienceMeasurementNotice});

  // Expectations.
  MockDesktopViewManagerObserver observer;
  desktop_view_manager()->AddObserver(&observer);
  EXPECT_CALL(observer, MaybeNavigateToNextStep(_)).Times(0);
  EXPECT_CALL(
      *mock_notice_service(),
      EventOccurred(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                                   SurfaceType::kDesktopNewTab),
                    PrivacySandboxNoticeEvent::kShown))
      .Times(1);

  // Shown event triggered.
  desktop_view_manager()->OnEventOccurred(
      PrivacySandboxNotice::kTopicsConsentNotice,
      PrivacySandboxNoticeEvent::kShown);
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kTopicsConsentNotice,
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));
}

TEST_F(DesktopViewManagerTest,
       OnEventOccurred_MultiplePendingNoticesNavigatesAllToNextNotice) {
  SetPendingNotices(
      {PrivacySandboxNotice::kTopicsConsentNotice,
       PrivacySandboxNotice::kProtectedAudienceMeasurementNotice});

  MockDesktopViewManagerObserver observer;
  desktop_view_manager()->AddObserver(&observer);
  EXPECT_CALL(observer,
              MaybeNavigateToNextStep(Optional(
                  PrivacySandboxNotice::kProtectedAudienceMeasurementNotice)))
      .Times(1);
  EXPECT_CALL(
      *mock_notice_service(),
      EventOccurred(std::make_pair(PrivacySandboxNotice::kTopicsConsentNotice,
                                   SurfaceType::kDesktopNewTab),
                    PrivacySandboxNoticeEvent::kOptIn))
      .Times(1);

  // Event triggered.
  desktop_view_manager()->OnEventOccurred(
      PrivacySandboxNotice::kTopicsConsentNotice,
      PrivacySandboxNoticeEvent::kOptIn);
  EXPECT_THAT(
      desktop_view_manager()->GetPendingNoticesToShow(),
      ElementsAre(PrivacySandboxNotice::kProtectedAudienceMeasurementNotice));
}

TEST_F(DesktopViewManagerTest,
       OnEventOccurred_OnePendingNoticeClosesAllNotice) {
  SetPendingNotices({PrivacySandboxNotice::kThreeAdsApisNotice});

  MockDesktopViewManagerObserver observer;
  desktop_view_manager()->AddObserver(&observer);
  EXPECT_CALL(
      *mock_notice_service(),
      EventOccurred(std::make_pair(PrivacySandboxNotice::kThreeAdsApisNotice,
                                   SurfaceType::kDesktopNewTab),
                    PrivacySandboxNoticeEvent::kAck))
      .Times(1);
  EXPECT_CALL(observer, MaybeNavigateToNextStep(Eq(std::nullopt))).Times(1);

  // First view creation.
  desktop_view_manager()->OnEventOccurred(
      PrivacySandboxNotice::kThreeAdsApisNotice,
      PrivacySandboxNoticeEvent::kAck);
  EXPECT_TRUE(desktop_view_manager()->GetPendingNoticesToShow().empty());
}

}  // namespace
}  // namespace privacy_sandbox
