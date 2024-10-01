// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_header.h"

#include <memory>

#include "ash/login/ui/fake_login_detachable_base_model.h"
#include "ash/login/ui/lock_contents_view_test_api.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_test_base.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/extended_updates/extended_updates_metrics.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/system/update/eol_notice_quick_settings_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/channel.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

EnterpriseDomainModel* GetEnterpriseDomainModel() {
  return Shell::Get()->system_tray_model()->enterprise_domain();
}

}  // namespace

class QuickSettingsHeaderTest : public NoSessionAshTestBase {
 public:
  QuickSettingsHeaderTest() = default;

  // AshTestBase:
  void SetUp() override {
    // Install a test delegate to allow overriding channel version.
    auto delegate = std::make_unique<TestShellDelegate>();
    test_shell_delegate_ = delegate.get();
    NoSessionAshTestBase::SetUp(std::move(delegate));

    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
  }

  void TearDown() override {
    header_ = nullptr;
    widget_.reset();
    controller_.reset();
    model_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // Creates the object under test. Not part of SetUp() because sometimes tests
  // need to setup the shell delegate or login before creating the header.
  void CreateQuickSettingsHeader() {
    // Instantiate view.
    auto header = std::make_unique<QuickSettingsHeader>(controller_.get());
    header_ = header.get();

    // Place the view in a large views::Widget so the buttons are clickable.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    widget_->SetContentsView(std::move(header));
  }

  views::View* GetManagedButton() {
    return header_->GetViewByID(VIEW_ID_QS_MANAGED_BUTTON);
  }

  views::Label* GetManagedButtonLabel() {
    return header_->GetManagedButtonLabelForTest();
  }

  views::View* GetSupervisedButton() {
    return header_->GetViewByID(VIEW_ID_QS_SUPERVISED_BUTTON);
  }

  views::Label* GetSupervisedButtonLabel() {
    return header_->GetSupervisedButtonLabelForTest();
  }

  raw_ptr<TestShellDelegate, DanglingUntriaged> test_shell_delegate_ = nullptr;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<QuickSettingsHeader> header_ = nullptr;
};

TEST_F(QuickSettingsHeaderTest, HiddenOnStable) {
  test_shell_delegate_->set_channel(version_info::Channel::STABLE);

  CreateQuickSettingsHeader();

  EXPECT_FALSE(GetManagedButton()->GetVisible());
  EXPECT_FALSE(GetSupervisedButton()->GetVisible());

  // Channel view is not created.
  EXPECT_FALSE(header_->channel_view_for_test());

  // Since no views are created, the header is hidden.
  EXPECT_FALSE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, ShowChannelViewBeforeLoginOnNonStable) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);

  CreateQuickSettingsHeader();

  EXPECT_TRUE(header_->channel_view_for_test());
  EXPECT_TRUE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, ShowsChannelViewAfterLogin) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);
  SimulateUserLogin("user@gmail.com");

  CreateQuickSettingsHeader();

  // Channel view is created.
  EXPECT_TRUE(header_->channel_view_for_test());

  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, EolNoticeVisible) {
  Shell::Get()->system_tray_model()->SetShowEolNotice(true);
  SimulateUserLogin("user@gmail.com");

  CreateQuickSettingsHeader();
  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());

  // EOL notice is visible.
  auto* eol_notice_view = header_->eol_notice_for_test();
  ASSERT_TRUE(eol_notice_view);
  EXPECT_TRUE(eol_notice_view->GetVisible());

  LeftClickOn(eol_notice_view);
  EXPECT_EQ(1, GetSystemTrayClient()->show_eol_info_count());
}

TEST_F(QuickSettingsHeaderTest, EolNoticeNotVisibleBeforeLogin) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);
  Shell::Get()->system_tray_model()->SetShowEolNotice(true);
  CreateQuickSettingsHeader();

  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());

  // Channel view is created.
  EXPECT_TRUE(header_->channel_view_for_test());

  // EOL notice is not visible.
  EXPECT_FALSE(header_->eol_notice_for_test());
}

TEST_F(QuickSettingsHeaderTest, ChannelIndicatorNotShownWithEolNotice) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);
  SimulateUserLogin("user@gmail.com");

  Shell::Get()->system_tray_model()->SetShowEolNotice(true);

  CreateQuickSettingsHeader();
  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());

  // No channel indicator.
  EXPECT_FALSE(header_->channel_view_for_test());

  // EOL notice is visible.
  ASSERT_TRUE(header_->eol_notice_for_test());
  EXPECT_TRUE(header_->eol_notice_for_test()->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, ExtendedUpdatesNoticeVisible) {
  Shell::Get()->system_tray_model()->SetShowExtendedUpdatesNotice(true);
  SimulateUserLogin("user@gmail.com");

  base::HistogramTester histogram_tester;
  CreateQuickSettingsHeader();

  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());
  histogram_tester.ExpectBucketCount(
      kExtendedUpdatesEntryPointEventMetric,
      ExtendedUpdatesEntryPointEvent::kQuickSettingsBannerShown, 1);

  // Extended Updates notice is visible.
  auto* extended_updates_notice_view = header_->GetExtendedUpdatesViewForTest();
  ASSERT_TRUE(extended_updates_notice_view);
  EXPECT_TRUE(extended_updates_notice_view->GetVisible());

  EXPECT_EQ(0, GetSystemTrayClient()->show_about_chromeos_count());
  LeftClickOn(extended_updates_notice_view);
  EXPECT_EQ(1, GetSystemTrayClient()->show_about_chromeos_count());
  histogram_tester.ExpectBucketCount(
      kExtendedUpdatesEntryPointEventMetric,
      ExtendedUpdatesEntryPointEvent::kQuickSettingsBannerClicked, 1);
}

TEST_F(QuickSettingsHeaderTest, ExtendedUpdatesNoticeNotVisibleBeforeLogin) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);
  Shell::Get()->system_tray_model()->SetShowExtendedUpdatesNotice(true);
  CreateQuickSettingsHeader();

  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());

  // Channel view is created.
  EXPECT_TRUE(header_->channel_view_for_test());

  // Extended Updates notice is not visible.
  EXPECT_FALSE(header_->GetExtendedUpdatesViewForTest());
}

TEST_F(QuickSettingsHeaderTest,
       ChannelIndicatorNotShownWithExtendedUpdatesNotice) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);
  SimulateUserLogin("user@gmail.com");

  Shell::Get()->system_tray_model()->SetShowExtendedUpdatesNotice(true);

  base::HistogramTester histogram_tester;
  CreateQuickSettingsHeader();

  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());

  // No channel indicator.
  EXPECT_FALSE(header_->channel_view_for_test());

  // Extended Updates notice is visible.
  ASSERT_TRUE(header_->GetExtendedUpdatesViewForTest());
  EXPECT_TRUE(header_->GetExtendedUpdatesViewForTest()->GetVisible());
  histogram_tester.ExpectBucketCount(
      kExtendedUpdatesEntryPointEventMetric,
      ExtendedUpdatesEntryPointEvent::kQuickSettingsBannerShown, 1);
}

TEST_F(QuickSettingsHeaderTest, ExtendedUpdatesNoticeNotShownWithEolNotice) {
  SimulateUserLogin("user@gmail.com");

  Shell::Get()->system_tray_model()->SetShowEolNotice(true);
  Shell::Get()->system_tray_model()->SetShowExtendedUpdatesNotice(true);

  base::HistogramTester histogram_tester;
  CreateQuickSettingsHeader();

  // Header is shown.
  EXPECT_TRUE(header_->GetVisible());

  // No channel indicator.
  EXPECT_FALSE(header_->channel_view_for_test());

  // EOL notice is visible.
  ASSERT_TRUE(header_->eol_notice_for_test());
  EXPECT_TRUE(header_->eol_notice_for_test()->GetVisible());

  // Extended Updates notice is not visible.
  EXPECT_FALSE(header_->GetExtendedUpdatesViewForTest());
  histogram_tester.ExpectBucketCount(
      kExtendedUpdatesEntryPointEventMetric,
      ExtendedUpdatesEntryPointEvent::kQuickSettingsBannerShown, 0);
}

TEST_F(QuickSettingsHeaderTest, EnterpriseManagedDeviceVisible) {
  CreateQuickSettingsHeader();

  // Simulate enterprise information becoming available.
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(DeviceEnterpriseInfo{
      "example.com", ManagementDeviceMode::kChromeEnterprise});

  EXPECT_TRUE(GetManagedButton()->GetVisible());
  EXPECT_EQ(GetManagedButtonLabel()->GetText(), u"Managed by example.com");
  EXPECT_EQ(GetManagedButton()->GetTooltipText({}), u"Managed by example.com");
  EXPECT_TRUE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, EnterpriseManagedAccountVisible) {
  CreateQuickSettingsHeader();

  // Simulate enterprise information becoming available.
  GetEnterpriseDomainModel()->SetEnterpriseAccountDomainInfo("example.com");

  EXPECT_TRUE(GetManagedButton()->GetVisible());
  EXPECT_EQ(GetManagedButtonLabel()->GetText(), u"Managed by example.com");
  EXPECT_EQ(GetManagedButton()->GetTooltipText({}), u"Managed by example.com");
  EXPECT_TRUE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, BothChannelAndEnterpriseVisible) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(DeviceEnterpriseInfo{
      "example.com", ManagementDeviceMode::kChromeEnterprise});
  SimulateUserLogin("user@gmail.com");

  CreateQuickSettingsHeader();

  EXPECT_TRUE(GetManagedButton()->GetVisible());
  // The label is the shorter "Managed" due to the two-column layout.
  EXPECT_EQ(GetManagedButtonLabel()->GetText(), u"Managed");
  EXPECT_EQ(GetManagedButton()->GetTooltipText({}), u"Managed by example.com");
  EXPECT_TRUE(header_->channel_view_for_test());
  EXPECT_TRUE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, BothEolNoticeAndEnterpriseVisible) {
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(DeviceEnterpriseInfo{
      "example.com", ManagementDeviceMode::kChromeEnterprise});
  Shell::Get()->system_tray_model()->SetShowEolNotice(true);
  SimulateUserLogin("user@gmail.com");

  CreateQuickSettingsHeader();

  EXPECT_TRUE(GetManagedButton()->GetVisible());
  // The label is the shorter "Managed" due to the two-column layout.
  EXPECT_EQ(GetManagedButtonLabel()->GetText(), u"Managed");
  EXPECT_EQ(GetManagedButton()->GetTooltipText({}), u"Managed by example.com");
  EXPECT_TRUE(header_->GetVisible());
  EolNoticeQuickSettingsView* eol_notice = header_->eol_notice_for_test();
  ASSERT_TRUE(eol_notice);
  EXPECT_TRUE(eol_notice->GetVisible());
  // The label is shorter due to the two-column layout.
  EXPECT_EQ(eol_notice->GetText(), u"Updates ended");

  LeftClickOn(eol_notice);
  EXPECT_EQ(1, GetSystemTrayClient()->show_eol_info_count());
}

TEST_F(QuickSettingsHeaderTest, ChildVisible) {
  CreateQuickSettingsHeader();

  // Before login the supervised user view is invisible.
  EXPECT_FALSE(GetSupervisedButton()->GetVisible());

  // Simulate supervised user logging in.
  SessionControllerImpl* session = Shell::Get()->session_controller();
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::UserType::kChild);
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  UserSession user_session = *session->GetUserSession(0);
  user_session.custodian_email = "parent@test.com";
  session->UpdateUserSession(std::move(user_session));

  // Recreate the header after login.
  CreateQuickSettingsHeader();

  // Now the supervised user view is visible.
  EXPECT_TRUE(GetSupervisedButton()->GetVisible());
  EXPECT_EQ(GetSupervisedButtonLabel()->GetText(), u"Supervised user");
  EXPECT_EQ(GetSupervisedButton()->GetTooltipText({}),
            u"Account managed by parent@test.com");
  EXPECT_TRUE(header_->GetVisible());

  LeftClickOn(GetSupervisedButton());
  EXPECT_EQ(GetSystemTrayClient()->show_account_settings_count(), 1);
}

TEST_F(QuickSettingsHeaderTest, ShowManagementDisclosure) {
  // This test relies on the lock screen actually being created (and creating
  // the lock screen requires the existence of an `AuthEventsRecorder`).
  std::unique_ptr<AuthEventsRecorder> auth_events_recorder =
      AuthEventsRecorder::CreateForTesting();

  // Setup to lock screen.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  GetSessionControllerClient()->set_show_lock_screen_views(true);
  client->LockScreen();
  client->SetSessionState(session_manager::SessionState::LOCKED);

  // Create new lock screen for lock_contents.
  ash::LockScreen::Get()->Destroy();
  LockScreen::Show(LockScreen::ScreenType::kLock);

  QuickSettingsHeader::ShowEnterpriseInfo(controller_.get(), true);
  LockContentsViewTestApi lock_contents(
      LockScreen::TestApi(LockScreen::Get()).contents_view());

  EXPECT_TRUE(lock_contents.management_disclosure_dialog());
}

TEST_F(QuickSettingsHeaderTest, DoNotShowManagementDisclosure) {
  // This test relies on the lock screen actually being created (and creating
  // the lock screen requires the existence of an `AuthEventsRecorder`).
  std::unique_ptr<AuthEventsRecorder> auth_events_recorder =
      AuthEventsRecorder::CreateForTesting();

  // Setup to lock screen.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  GetSessionControllerClient()->set_show_lock_screen_views(true);
  client->LockScreen();
  client->SetSessionState(session_manager::SessionState::LOCKED);

  // Create new lock screen for lock_contents.
  ash::LockScreen::Get()->Destroy();
  LockScreen::Show(LockScreen::ScreenType::kLock);

  QuickSettingsHeader::ShowEnterpriseInfo(controller_.get(), false);
  LockContentsViewTestApi lock_contents(
      LockScreen::TestApi(LockScreen::Get()).contents_view());

  EXPECT_FALSE(lock_contents.management_disclosure_dialog());
}

}  // namespace ash
