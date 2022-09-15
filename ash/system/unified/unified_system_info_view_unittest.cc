// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_system_info_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/system_tray_client.h"
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
#include "components/version_info/channel.h"

namespace ash {

// `UnifiedSystemInfoView` contains a set of "baseline" UI elements that are
// always visible, but some elements are visible only under certain conditions.
// To verify that these "conditional" UI elements are visible or not-visible
// only when expected, each `UnifiedSystemInfoViewTest` test case is executed
// with every possible combination of the following flags, passed as a
// parameter.
enum class TestFlags : uint8_t {
  // No conditional UI flags are set.
  kNone = 0b00000000,

  // Enterprise/management status display is enabled.
  kManagedDeviceUi = 0b00000001,

  // Release track UI is visible if two conditions are met: (1) the feature that
  // guards its display is enabled (kReleaseTrackUi) and (2) the release track
  // itself is a value other than "stable" (kReleaseTrackNotStable). Each
  // combination of one, none, or both of these conditions is a valid scenario.
  kReleaseTrackUi = 0b00000010,
  kReleaseTrackNotStable = 0b00000100,
};

TestFlags operator&(TestFlags a, TestFlags b) {
  return static_cast<TestFlags>(static_cast<uint8_t>(a) &
                                static_cast<uint8_t>(b));
}

TestFlags operator|(TestFlags a, TestFlags b) {
  return static_cast<TestFlags>(static_cast<uint8_t>(a) |
                                static_cast<uint8_t>(b));
}

class UnifiedSystemInfoViewTest
    : public AshTestBase,
      public testing::WithParamInterface<TestFlags> {
 public:
  UnifiedSystemInfoViewTest() = default;
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

    // Enable/disable of the two features we care about is conditional on the
    // passed-in parameter.
    scoped_feature_list_ = std::make_unique<base::test::ScopedFeatureList>();
    std::vector<base::Feature> enabled_features, disabled_features;
    if (IsManagedDeviceUIRedesignEnabled())
      enabled_features.push_back(features::kManagedDeviceUIRedesign);
    else
      disabled_features.push_back(features::kManagedDeviceUIRedesign);
    if (IsReleaseTrackUiEnabled())
      enabled_features.push_back(features::kReleaseTrackUi);
    else
      disabled_features.push_back(features::kReleaseTrackUi);
    scoped_feature_list_->InitWithFeatures(enabled_features, disabled_features);

    // Instantiate members.
    model_ = base::MakeRefCounted<UnifiedSystemTrayModel>(nullptr);
    controller_ = std::make_unique<UnifiedSystemTrayController>(model_.get());
    info_view_ = std::make_unique<UnifiedSystemInfoView>(controller_.get());
  }

  bool IsManagedDeviceUIRedesignEnabled() const {
    return (GetParam() & TestFlags::kManagedDeviceUi) != TestFlags::kNone;
  }

  bool IsReleaseTrackUiEnabled() const {
    return (GetParam() & TestFlags::kReleaseTrackUi) != TestFlags::kNone;
  }

  bool IsReleaseTrackNotStable() const {
    return (GetParam() & TestFlags::kReleaseTrackNotStable) != TestFlags::kNone;
  }

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

  void TearDown() override {
    info_view_.reset();
    controller_.reset();
    model_.reset();
    scoped_feature_list_.reset();
    AshTestBase::TearDown();
  }

 protected:
  UnifiedSystemInfoView* info_view() { return info_view_.get(); }
  EnterpriseDomainModel* enterprise_domain() {
    return Shell::Get()->system_tray_model()->enterprise_domain();
  }

 private:
  scoped_refptr<UnifiedSystemTrayModel> model_;
  std::unique_ptr<UnifiedSystemTrayController> controller_;
  std::unique_ptr<UnifiedSystemInfoView> info_view_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Execute each test case with every possible combination of `TestFlags`.
INSTANTIATE_TEST_SUITE_P(
    All,
    UnifiedSystemInfoViewTest,
    testing::Values(TestFlags::kNone,
                    TestFlags::kManagedDeviceUi,
                    TestFlags::kReleaseTrackUi,
                    TestFlags::kManagedDeviceUi | TestFlags::kReleaseTrackUi,
                    TestFlags::kReleaseTrackNotStable,
                    TestFlags::kManagedDeviceUi |
                        TestFlags::kReleaseTrackNotStable,
                    TestFlags::kReleaseTrackUi |
                        TestFlags::kReleaseTrackNotStable,
                    TestFlags::kManagedDeviceUi | TestFlags::kReleaseTrackUi |
                        TestFlags::kReleaseTrackNotStable));

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

  // If the release track UI is enabled AND the release track is non-stable, the
  // version button is shown.
  EXPECT_EQ(IsReleaseTrackUiEnabled() && IsReleaseTrackNotStable(),
            GetVersionButton() && GetVersionButton()->GetVisible());

  // If the release track UI is enabled AND the release track is non-stable AND
  // the user feedback is enabled, the feedback button is shown.
  EXPECT_EQ(
      IsReleaseTrackUiEnabled() && IsReleaseTrackNotStable() &&
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

  // If the release track UI is enabled AND the release track is non-stable, the
  // version button is shown.
  EXPECT_EQ(IsReleaseTrackUiEnabled() && IsReleaseTrackNotStable(),
            GetVersionButton() && GetVersionButton()->GetVisible());

  // If the release track UI is enabled AND the release track is non-stable AND
  // the user feedback is enabled, the feedback button is shown.
  EXPECT_EQ(
      IsReleaseTrackUiEnabled() && IsReleaseTrackNotStable() &&
          Shell::Get()->system_tray_model()->client()->IsUserFeedbackEnabled(),
      GetFeedbackButton() && GetFeedbackButton()->GetVisible());
}

TEST_P(UnifiedSystemInfoViewTest, EnterpriseUserManagedVisible) {
  // By default, EnterpriseManagedView is not shown.
  EXPECT_FALSE(GetManagedButton()->GetVisible());

  // Simulate enterprise information becoming available.
  enterprise_domain()->SetEnterpriseAccountDomainInfo("example.com");

  // EnterpriseManagedView should be shown if the feature is enabled.
  EXPECT_EQ(IsManagedDeviceUIRedesignEnabled(),
            GetManagedButton()->GetVisible());

  // If the release track UI is enabled AND the release track is non-stable, the
  // version button is shown.
  EXPECT_EQ(IsReleaseTrackUiEnabled() && IsReleaseTrackNotStable(),
            GetVersionButton() && GetVersionButton()->GetVisible());

  // If the release track UI is enabled AND the release track is non-stable AND
  // the user feedback is enabled, the feedback button is shown.
  EXPECT_EQ(
      IsReleaseTrackUiEnabled() && IsReleaseTrackNotStable() &&
          Shell::Get()->system_tray_model()->client()->IsUserFeedbackEnabled(),
      GetFeedbackButton() && GetFeedbackButton()->GetVisible());
}

using UnifiedSystemInfoViewNoSessionTest = NoSessionAshTestBase;

TEST_F(UnifiedSystemInfoViewNoSessionTest, ChildVisible) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kManagedDeviceUIRedesign);
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
