// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_header.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/buttons.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/check.h"
#include "base/test/scoped_feature_list.h"
#include "components/user_manager/user_type.h"
#include "components/version_info/channel.h"
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
  QuickSettingsHeaderTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});
  }

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
    header_.reset();
    controller_.reset();
    model_.reset();
    NoSessionAshTestBase::TearDown();
  }

  // Creates the object under test. Not part of SetUp() because sometimes tests
  // need to setup the shell delegate or login before creating the header.
  void CreateQuickSettingsHeader() {
    header_ = std::make_unique<QuickSettingsHeader>(controller_.get());
  }

  views::View* GetManagedButton() {
    return header_->GetViewByID(VIEW_ID_QS_MANAGED_BUTTON);
  }

  views::Label* GetManagedButtonLabel() {
    views::View* view = GetManagedButton();
    DCHECK(views::IsViewClass<EnterpriseManagedView>(view));
    return views::AsViewClass<EnterpriseManagedView>(view)->label();
  }

  views::View* GetSupervisedButton() {
    return header_->GetViewByID(VIEW_ID_QS_SUPERVISED_BUTTON);
  }

  views::Label* GetSupervisedButtonLabel() {
    views::View* view = GetSupervisedButton();
    DCHECK(views::IsViewClass<SupervisedUserView>(view));
    return views::AsViewClass<SupervisedUserView>(view)->label();
  }

  base::test::ScopedFeatureList feature_list_;
  TestShellDelegate* test_shell_delegate_ = nullptr;
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<QuickSettingsHeader> header_;
};

TEST_F(QuickSettingsHeaderTest, HiddenByDefaultBeforeLogin) {
  CreateQuickSettingsHeader();

  EXPECT_FALSE(GetManagedButton()->GetVisible());
  EXPECT_FALSE(GetSupervisedButton()->GetVisible());

  // By default, channel view is not created.
  EXPECT_FALSE(header_->channel_view_for_test());

  // Since no views are created, the header is hidden.
  EXPECT_FALSE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, DoesNotShowChannelViewBeforeLogin) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);

  CreateQuickSettingsHeader();

  EXPECT_FALSE(header_->channel_view_for_test());
  EXPECT_FALSE(header_->GetVisible());
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

TEST_F(QuickSettingsHeaderTest, EnterpriseManagedDeviceVisible) {
  CreateQuickSettingsHeader();

  // Simulate enterprise information becoming available.
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"example.com", /*active_directory_managed=*/false,
                           ManagementDeviceMode::kChromeEnterprise});

  EXPECT_TRUE(GetManagedButton()->GetVisible());
  EXPECT_EQ(GetManagedButtonLabel()->GetText(), u"Managed by example.com");
  EXPECT_EQ(GetManagedButton()->GetTooltipText({}), u"Managed by example.com");
  EXPECT_TRUE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, EnterpriseManagedActiveDirectoryVisible) {
  CreateQuickSettingsHeader();

  // Simulate enterprise information becoming available.
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"", /*active_directory_managed=*/true,
                           ManagementDeviceMode::kChromeEnterprise});

  EXPECT_TRUE(GetManagedButton()->GetVisible());
  // Active Directory just shows "Managed" as the button label.
  EXPECT_EQ(GetManagedButtonLabel()->GetText(), u"Managed");
  EXPECT_EQ(GetManagedButton()->GetTooltipText({}),
            u"This Chrome device is enterprise managed");
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
  GetEnterpriseDomainModel()->SetDeviceEnterpriseInfo(
      DeviceEnterpriseInfo{"example.com", /*active_directory_managed=*/false,
                           ManagementDeviceMode::kChromeEnterprise});
  SimulateUserLogin("user@gmail.com");

  CreateQuickSettingsHeader();

  EXPECT_TRUE(GetManagedButton()->GetVisible());
  // The label is the shorter "Managed" due to the two-column layout.
  EXPECT_EQ(GetManagedButtonLabel()->GetText(), u"Managed");
  EXPECT_EQ(GetManagedButton()->GetTooltipText({}), u"Managed by example.com");
  EXPECT_TRUE(header_->channel_view_for_test());
  EXPECT_TRUE(header_->GetVisible());
}

TEST_F(QuickSettingsHeaderTest, ChildVisible) {
  CreateQuickSettingsHeader();

  // Before login the supervised user view is invisible.
  EXPECT_FALSE(GetSupervisedButton()->GetVisible());

  // Simulate supervised user logging in.
  SessionControllerImpl* session = Shell::Get()->session_controller();
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->Reset();
  client->AddUserSession("child@test.com", user_manager::USER_TYPE_CHILD);
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
}

}  // namespace ash
