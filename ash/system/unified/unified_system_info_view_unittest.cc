// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_info_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/version_info/channel.h"
#include "ui/views/test/ax_event_counter.h"

namespace ash {

// Tests are parameterized by the release track UI:
// - Whether the release track UI feature is enabled, and
// - Whether the release track is a value other than "stable"
// - Whether EOL notice is expected to be shown.
// The release track UI only shows if both conditions are met.
//
// NOTE: For QsRevamp, see similar tests in QuickSettingsHeaderTest.
class UnifiedSystemInfoViewTest
    : public AshTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  UnifiedSystemInfoViewTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  UnifiedSystemInfoViewTest(const UnifiedSystemInfoViewTest&) = delete;
  UnifiedSystemInfoViewTest& operator=(const UnifiedSystemInfoViewTest&) =
      delete;
  ~UnifiedSystemInfoViewTest() override = default;

  void SetUp() override {
    // Provide our own `TestShellDelegate`, with a non-stable channel set if
    // the passed-in parameter dictates.
    std::unique_ptr<TestShellDelegate> shell_delegate =
        std::make_unique<TestShellDelegate>();
    if (IsReleaseTrackNotStable())
      shell_delegate->set_channel(version_info::Channel::BETA);
    AshTestBase::SetUp(std::move(shell_delegate));

    if (ShouldShowEolNotice()) {
      Shell::Get()->system_tray_model()->SetShowEolNotice(true);
    }

    // Enable/disable features based on the passed-in parameter.
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitWithFeatureState(features::kReleaseTrackUi,
                                               IsReleaseTrackUiEnabled());

    // Instantiate members.
    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
    auto info_view = std::make_unique<UnifiedSystemInfoView>(controller_.get());
    info_view_ = info_view.get();

    // Place the view in a large views::Widget so the buttons are clickable.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(std::move(info_view));
  }

  bool IsReleaseTrackUiEnabled() const { return std::get<0>(GetParam()); }

  bool IsReleaseTrackNotStable() const { return std::get<1>(GetParam()); }

  bool ShouldShowEolNotice() const { return std::get<2>(GetParam()); }

  views::View* GetDateButton() {
    return info_view_->GetViewByID(VIEW_ID_QS_DATE_VIEW_BUTTON);
  }

  views::View* GetBatteryButton() {
    return info_view_->GetViewByID(VIEW_ID_QS_BATTERY_BUTTON);
  }

  views::View* GetManagedButton() {
    return info_view_->GetViewByID(VIEW_ID_QS_MANAGED_BUTTON);
  }

  views::View* GetVersionButton() {
    return info_view_->GetViewByID(VIEW_ID_QS_VERSION_BUTTON);
  }

  views::View* GetFeedbackButton() {
    return info_view_->GetViewByID(VIEW_ID_QS_FEEDBACK_BUTTON);
  }

  views::View* GetEolNoticeButton() {
    return info_view_->GetViewByID(VIEW_ID_QS_EOL_NOTICE_BUTTON);
  }

  void TearDown() override {
    info_view_ = nullptr;
    widget_.reset();
    controller_.reset();
    model_.reset();
    scoped_feature_list_.reset();
    AshTestBase::TearDown();
  }

 protected:
  UnifiedSystemInfoView* info_view() { return info_view_; }
  EnterpriseDomainModel* enterprise_domain() {
    return Shell::Get()->system_tray_model()->enterprise_domain();
  }

 private:
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  UnifiedSystemInfoView* info_view_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnifiedSystemInfoViewTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

TEST_P(UnifiedSystemInfoViewTest, ButtonNameAndVisibility) {
  // By default, EnterpriseManagedView is not shown.
  EXPECT_FALSE(GetManagedButton()->GetVisible());

  // Simulate enterprise information becoming available.
  enterprise_domain()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"example.com", /*active_directory_managed=*/false,
                           ManagementDeviceMode::kChromeEnterprise});

  // EnterpriseManagedView should be shown.
  EXPECT_TRUE(GetManagedButton()->GetVisible());

  // `DateView` should be shown.
  EXPECT_TRUE(GetDateButton()->GetVisible());

  // Battery button should be shown.
  EXPECT_TRUE(GetBatteryButton()->GetVisible());

  EXPECT_EQ(ShouldShowEolNotice(),
            GetEolNoticeButton() && GetEolNoticeButton()->GetVisible());
  if (ShouldShowEolNotice()) {
    LeftClickOn(GetEolNoticeButton());
    EXPECT_EQ(1, GetSystemTrayClient()->show_eol_info_count());
  }

  const bool show_release_track_info = IsReleaseTrackUiEnabled() &&
                                       IsReleaseTrackNotStable() &&
                                       !ShouldShowEolNotice();

  // If the release track UI is enabled AND the release track is non-stable, the
  // version button is shown.
  EXPECT_EQ(show_release_track_info,
            GetVersionButton() && GetVersionButton()->GetVisible());

  // If the release track UI is enabled AND the release track is non-stable AND
  // the user feedback is enabled, the feedback button is shown.
  EXPECT_EQ(
      show_release_track_info &&
          Shell::Get()->system_tray_model()->client()->IsUserFeedbackEnabled(),
      GetFeedbackButton() && GetFeedbackButton()->GetVisible());
}

TEST_P(UnifiedSystemInfoViewTest, EnterpriseManagedVisibleForActiveDirectory) {
  // Active directory information becoming available.
  const std::string empty_domain;
  enterprise_domain()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{empty_domain, /*active_directory_managed=*/true,
                           ManagementDeviceMode::kChromeEnterprise});

  // EnterpriseManagedView should be shown.
  EXPECT_TRUE(GetManagedButton()->GetVisible());

  EXPECT_EQ(ShouldShowEolNotice(),
            GetEolNoticeButton() && GetEolNoticeButton()->GetVisible());

  if (ShouldShowEolNotice()) {
    LeftClickOn(GetEolNoticeButton());
    EXPECT_EQ(1, GetSystemTrayClient()->show_eol_info_count());
  }

  const bool show_release_track_info = IsReleaseTrackUiEnabled() &&
                                       IsReleaseTrackNotStable() &&
                                       !ShouldShowEolNotice();

  // If the release track UI is enabled AND the release track is non-stable, the
  // version button is shown.
  EXPECT_EQ(show_release_track_info,
            GetVersionButton() && GetVersionButton()->GetVisible());

  // If the release track UI is enabled AND the release track is non-stable AND
  // the user feedback is enabled, the feedback button is shown.
  EXPECT_EQ(
      show_release_track_info &&
          Shell::Get()->system_tray_model()->client()->IsUserFeedbackEnabled(),
      GetFeedbackButton() && GetFeedbackButton()->GetVisible());
}

TEST_P(UnifiedSystemInfoViewTest, EnterpriseUserManagedVisible) {
  // By default, EnterpriseManagedView is not shown.
  EXPECT_FALSE(GetManagedButton()->GetVisible());

  // Simulate enterprise information becoming available.
  enterprise_domain()->SetEnterpriseAccountDomainInfo("example.com");

  // EnterpriseManagedView should be shown.
  EXPECT_TRUE(GetManagedButton()->GetVisible());

  EXPECT_EQ(ShouldShowEolNotice(),
            GetEolNoticeButton() && GetEolNoticeButton()->GetVisible());

  if (ShouldShowEolNotice()) {
    LeftClickOn(GetEolNoticeButton());
    EXPECT_EQ(1, GetSystemTrayClient()->show_eol_info_count());
  }

  const bool show_release_track_info = IsReleaseTrackUiEnabled() &&
                                       IsReleaseTrackNotStable() &&
                                       !ShouldShowEolNotice();

  // If the release track UI is enabled AND the release track is non-stable, the
  // version button is shown.
  EXPECT_EQ(show_release_track_info,
            GetVersionButton() && GetVersionButton()->GetVisible());

  // If the release track UI is enabled AND the release track is non-stable AND
  // the user feedback is enabled, the feedback button is shown.
  EXPECT_EQ(
      show_release_track_info &&
          Shell::Get()->system_tray_model()->client()->IsUserFeedbackEnabled(),
      GetFeedbackButton() && GetFeedbackButton()->GetVisible());
}

TEST_P(UnifiedSystemInfoViewTest, UpdateFiresAccessibilityEvents) {
  // Set the current time to 08:00 for testing.
  task_environment()->AdvanceClock(base::Time::Now().LocalMidnight() +
                                   base::Hours(32) - base::Time::Now());
  views::test::AXEventCounter counter(views::AXEventManager::Get());
  auto* date_view = info_view()->GetDateViewForTesting();
  auto* date_view_label = info_view()->GetDateViewLabelForTesting();
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged, date_view));
  EXPECT_EQ(0,
            counter.GetCount(ax::mojom::Event::kTextChanged, date_view_label));

  // `DateView::Update` sets the accessible name of both itself and its label.
  // This will result in text-changed events being emitted, but only if the
  // accessible name has actually changed. Therefore advance the clock by a
  // minute before calling `Update`.
  task_environment()->FastForwardBy(base::Minutes(1));
  info_view()->UpdateDateViewForTesting();
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged, date_view));
  EXPECT_EQ(1,
            counter.GetCount(ax::mojom::Event::kTextChanged, date_view_label));
}

using UnifiedSystemInfoViewNoSessionTest = NoSessionAshTestBase;

TEST_F(UnifiedSystemInfoViewNoSessionTest, ChildVisible) {
  auto model = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
  auto controller = std::make_unique<UnifiedSystemTrayController>(model.get());

  SessionControllerImpl* session = Shell::Get()->session_controller();
  ASSERT_FALSE(session->IsActiveUserSessionStarted());

  // Before login the supervised user view is invisible.
  {
    auto info_view = std::make_unique<UnifiedSystemInfoView>(controller.get());
    EXPECT_FALSE(info_view->IsSupervisedVisibleForTesting());
  }

  // Simulate a supervised user logging in.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::USER_TYPE_CHILD);
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  UserSession user_session = *session->GetUserSession(0);
  user_session.custodian_email = "parent@test.com";
  session->UpdateUserSession(std::move(user_session));

  // Now the supervised user view is visible.
  {
    auto info_view = std::make_unique<UnifiedSystemInfoView>(controller.get());
    EXPECT_TRUE(info_view->IsSupervisedVisibleForTesting());
  }
}

}  // namespace ash
