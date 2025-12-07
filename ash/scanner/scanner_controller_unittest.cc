// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/public/cpp/scanner/scanner_enums.h"
#include "ash/public/cpp/scanner/scanner_feedback_info.h"
#include "ash/public/cpp/system/scoped_toast_pause.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/scanner/fake_scanner_delegate.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_action_view_model.h"
#include "ash/scanner/scanner_enterprise_policy.h"
#include "ash/scanner/scanner_metrics.h"
#include "ash/scanner/scanner_session.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/toast/toast_overlay.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_util.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_split.h"
#include "base/strings/string_view_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "components/account_id/account_id.h"
#include "components/feedback/feedback_constants.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/scanner.pb.h"
#include "components/manta/scanner_provider.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/fake_message_center.h"
#include "ui/message_center/message_center.h"
#include "ui/views/view_utils.h"
#include "ui/wm/core/window_util.h"
#include "url/gurl.h"

namespace ash {

namespace {

using ::base::test::EqualsProto;
using ::base::test::InvokeFuture;
using ::base::test::IsJson;
using ::base::test::RunOnceCallback;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StartsWith;
using ::testing::WithArg;

constexpr char kScannerActionSuccessToastId[] = "scanner_action_success";
constexpr char kScannerActionFailureToastId[] = "scanner_action_failure";

FakeScannerDelegate* GetFakeScannerDelegate(
    ScannerController& scanner_controller) {
  return static_cast<FakeScannerDelegate*>(
      scanner_controller.delegate_for_testing());
}

FakeScannerProfileScopedDelegate* GetFakeScannerProfileScopedDelegate(
    ScannerController& scanner_controller) {
  return static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller.delegate_for_testing()->GetProfileScopedDelegate());
}

scoped_refptr<base::RefCountedMemory> MakeJpegBytes(int width = 100,
                                                    int height = 100) {
  gfx::ImageSkia img = gfx::test::CreateImageSkia(width, height);
  std::optional<std::vector<uint8_t>> data =
      gfx::JPEGCodec::Encode(*img.bitmap(), /*quality=*/90);
  CHECK(data.has_value());
  return base::MakeRefCounted<base::RefCountedBytes>(std::move(*data));
}

class MockNewWindowDelegate : public TestNewWindowDelegate {
 public:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

// Only used in ScannerControllerNoFixtureTest.
class MockToastManager : public ToastManager {
 public:
  MOCK_METHOD(void, Show, (ToastData data), (override));
  MOCK_METHOD(void, Cancel, (std::string_view id), (override));
  MOCK_METHOD(bool,
              RequestFocusOnActiveToastButton,
              (std::string_view id),
              (override));
  MOCK_METHOD(bool, IsToastShown, (std::string_view id), (const override));
  MOCK_METHOD(bool,
              IsToastButtonFocused,
              (std::string_view id),
              (const override));
  MOCK_METHOD(std::unique_ptr<ScopedToastPause>,
              CreateScopedPause,
              (),
              (override));
  MOCK_METHOD(void, Pause, (), (override));
  MOCK_METHOD(void, Resume, (), (override));
};

class MockScannerDelegate : public ScannerDelegate {
 public:
  MOCK_METHOD(ScannerProfileScopedDelegate*,
              GetProfileScopedDelegate,
              (),
              (override));
  MOCK_METHOD(void,
              OpenFeedbackDialog,
              (const AccountId& account_id,
               ScannerFeedbackInfo feedback_info,
               SendFeedbackCallback send_feedback_callback),
              (override));
};

class ScannerControllerTest : public AshTestBase {
 public:
  ScannerControllerTest() = default;
  ScannerControllerTest(const ScannerControllerTest&) = delete;
  ScannerControllerTest& operator=(const ScannerControllerTest&) = delete;
  ~ScannerControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    auto shell_delegate = std::make_unique<TestShellDelegate>();
    shell_delegate->SetSendSpecializedFeatureFeedbackCallback(
        mock_send_specialized_feature_feedback_.Get());
    set_shell_delegate(std::move(shell_delegate));
    AshTestBase::SetUp();
  }

  base::MockCallback<TestShellDelegate::SendSpecializedFeatureFeedbackCallback>&
  mock_send_specialized_feature_feedback() {
    return mock_send_specialized_feature_feedback_;
  }

  MockNewWindowDelegate& mock_new_window_delegate() {
    return mock_new_window_delegate_;
  }

 private:
  testing::StrictMock<base::MockCallback<
      TestShellDelegate::SendSpecializedFeatureFeedbackCallback>>
      mock_send_specialized_feature_feedback_;
  MockNewWindowDelegate mock_new_window_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_{features::kScannerUpdate};
};

TEST_F(ScannerControllerTest, CanStartSessionIfFeatureChecksPass) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));

  EXPECT_TRUE(scanner_controller->CanStartSession());
  EXPECT_TRUE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest, CanNotStartSessionIfFeatureChecksFail) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kDisabledInSettings}));

  EXPECT_FALSE(scanner_controller->CanStartSession());
  EXPECT_FALSE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest,
       CanStartSessionIfEnterprisePolicyAllowedWithModelImprovement) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(ScannerEnterprisePolicy::kAllowedWithModelImprovement));

  EXPECT_TRUE(scanner_controller->CanStartSession());
  EXPECT_TRUE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest,
       CanStartSessionIfEnterprisePolicyAllowedWithoutModelImprovement) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(
          ScannerEnterprisePolicy::kAllowedWithoutModelImprovement));

  EXPECT_TRUE(scanner_controller->CanStartSession());
  EXPECT_TRUE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest, CanStartSessionIfEnterprisePolicyIsInvalidValue) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed, 3);

  EXPECT_TRUE(scanner_controller->CanStartSession());
  EXPECT_TRUE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest,
       CannotStartSessionIfDisallowedByEnterprisePolicy) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(ScannerEnterprisePolicy::kDisallowed));

  EXPECT_FALSE(scanner_controller->CanStartSession());
  EXPECT_FALSE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest, CannotStartSessionInPinnedMode) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));

  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/false);
  ASSERT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());

  EXPECT_FALSE(scanner_controller->CanStartSession());
  EXPECT_FALSE(scanner_controller->StartNewSession());
}

TEST_F(ScannerControllerTest, CanShowFeatureSettingsToggleIfNoChecksFail) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));

  EXPECT_TRUE(scanner_controller->CanShowFeatureSettingsToggle());
}

TEST_F(ScannerControllerTest, DoesNotShowFeatureSettingsToggleInPinnedMode) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));

  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/false);
  ASSERT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());

  EXPECT_FALSE(scanner_controller->CanShowFeatureSettingsToggle());
}

TEST_F(ScannerControllerTest, CanShowUiIfConsentNotAcceptedOnly) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kConsentNotAccepted}));

  EXPECT_TRUE(scanner_controller->CanShowUi());
  EXPECT_TRUE(ScannerController::CanShowUiForShell());
}

TEST_F(ScannerControllerTest,
       CanShowUiIfEnterprisePolicyAllowedWithModelImprovement) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(ScannerEnterprisePolicy::kAllowedWithModelImprovement));

  EXPECT_TRUE(scanner_controller->CanShowUi());
  EXPECT_TRUE(ScannerController::CanShowUiForShell());
}

TEST_F(ScannerControllerTest,
       CanShowUiIfEnterprisePolicyAllowedWithoutModelImprovement) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(
          ScannerEnterprisePolicy::kAllowedWithoutModelImprovement));

  EXPECT_TRUE(scanner_controller->CanShowUi());
  EXPECT_TRUE(ScannerController::CanShowUiForShell());
}

TEST_F(ScannerControllerTest, CanShowUiIfEnterprisePolicyIsInvalidValue) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed, 3);

  EXPECT_TRUE(scanner_controller->CanShowUi());
  EXPECT_TRUE(ScannerController::CanShowUiForShell());
}

TEST_F(ScannerControllerTest, CannotShowUiIfDisallowedByEnterprisePolicy) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(ScannerEnterprisePolicy::kDisallowed));

  EXPECT_FALSE(scanner_controller->CanShowUi());
  EXPECT_FALSE(ScannerController::CanShowUiForShell());
}

TEST(ScannerControllerNoFixtureTest, CanShowUiForShellFalseWhenNoShell) {
  ASSERT_FALSE(Shell::HasInstance());
  EXPECT_FALSE(ScannerController::CanShowUiForShell());
}

class ScannerControllerDisabledTest : public AshTestBase {
 public:
  ScannerControllerDisabledTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, /*disabled_features=*/{
            features::kScannerUpdate, features::kScannerDogfood});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ScannerControllerDisabledTest, CanShowUiForShellFalseWhenNoController) {
  ASSERT_FALSE(Shell::Get()->scanner_controller());
  EXPECT_FALSE(ScannerController::CanShowUiForShell());
}

TEST_F(ScannerControllerTest, CannotShowUiInPinnedMode) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));

  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/false);
  ASSERT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());

  EXPECT_FALSE(scanner_controller->CanShowUi());
  EXPECT_FALSE(ScannerController::CanShowUiForShell());
}

TEST_F(ScannerControllerTest, CanShowUiAfterExitingPinnedMode) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/false);
  ASSERT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());
  ASSERT_FALSE(scanner_controller->CanShowUi());
  ASSERT_FALSE(ScannerController::CanShowUiForShell());

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kUnpin, {});

  EXPECT_TRUE(scanner_controller->CanShowUi());
  EXPECT_TRUE(ScannerController::CanShowUiForShell());
}

TEST(ScannerControllerNoFixtureTest, CanShowUiForShellFalseWhenNoShellMetrics) {
  base::HistogramTester histogram_tester;

  ASSERT_FALSE(Shell::HasInstance());
  ASSERT_FALSE(ScannerController::CanShowUiForShell());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToNoShellInstance, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerDisabledTest,
       CanShowUiForShellFalseWhenNoControllerMetrics) {
  base::HistogramTester histogram_tester;

  ASSERT_FALSE(Shell::Get()->scanner_controller());
  ASSERT_FALSE(ScannerController::CanShowUiForShell());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToNoControllerOnShell,
      1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest, CanShowUiForShellFalseWhenPinnedMetrics) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  std::unique_ptr<aura::Window> pinned_window = CreateAppWindow();
  wm::ActivateWindow(pinned_window.get());
  window_util::PinWindow(pinned_window.get(), /*trusted=*/false);
  ASSERT_TRUE(Shell::Get()->screen_pinning_controller()->IsPinned());
  // This must be after pinning, as `SunfishScannerFeatureWatcher` may
  // automatically call `CanShowUi` when the pinned state changes.
  base::HistogramTester histogram_tester;

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToPinnedMode, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST(ScannerControllerNoFixtureTest,
     CanShowUiFalseWhenNoProfileScopedDelegateMetrics) {
  base::HistogramTester histogram_tester;
  SessionControllerImpl session_controller;
  auto mock_delegate = std::make_unique<MockScannerDelegate>();
  EXPECT_CALL(*mock_delegate, GetProfileScopedDelegate())
      .WillRepeatedly(Return(nullptr));
  ScannerController scanner_controller(std::move(mock_delegate),
                                       session_controller,
                                       /*screen_pinning_controller=*/nullptr);

  ASSERT_FALSE(scanner_controller.CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::
          kCanShowUiReturnedFalseDueToNoProfileScopedDelegate,
      1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest,
       CanShowUiFalseWhenEnterprisePolicyDisallowedMetrics) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(ScannerEnterprisePolicy::kDisallowed));
  // This must be after setting the preference, as some preference observers
  // like `SunfishScannerFeatureWatcher` may automatically call `CanShowUi`.
  base::HistogramTester histogram_tester;

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToEnterprisePolicy, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest, CanShowUiFalseWhenSettingsToggleDisabledMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kDisabledInSettings}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToSettingsToggle, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest, CanShowUiFalseWhenFeatureFlagDisabledMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kFeatureFlagDisabled}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToFeatureFlag, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest,
       CanShowUiFalseWhenFeatureManagementCheckFailedMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::
              kFeatureManagementCheckFailed}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToFeatureManagement,
      1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest, CanShowUiFalseWhenSecretKeyCheckFailedMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kSecretKeyCheckFailed}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToSecretKey, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest, CanShowUiFalseWhenCountryCheckFailedMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kCountryCheckFailed}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToCountry, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest, CanShowUiFalseWhenKioskModeMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::
              kDisabledInKioskModeCheckFailed}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToKioskMode, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest,
       CanShowUiFalseMultipleFeatureAccessCheckFailsWithoutConsentMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::
              kFeatureManagementCheckFailed,
          specialized_features::FeatureAccessFailure::
              kAccountCapabilitiesCheckFailed,
          specialized_features::FeatureAccessFailure::kCountryCheckFailed}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToFeatureManagement,
      1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToAccountCapabilities,
      1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToCountry, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest,
       CanShowUiFalseMultipleFeatureAccessCheckFailsWithConsentMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kConsentNotAccepted,
          specialized_features::FeatureAccessFailure::
              kFeatureManagementCheckFailed,
          specialized_features::FeatureAccessFailure::
              kAccountCapabilitiesCheckFailed,
          specialized_features::FeatureAccessFailure::kCountryCheckFailed}));

  ASSERT_FALSE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToFeatureManagement,
      1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToAccountCapabilities,
      1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalseDueToCountry, 1);
  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedFalse, 1);
}

TEST_F(ScannerControllerTest, CanShowUiTrueWithoutConsentMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kConsentNotAccepted}));

  ASSERT_TRUE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedTrueWithoutConsent, 1);
}

TEST_F(ScannerControllerTest, CanShowUiTrueWithConsentMetrics) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              CheckFeatureAccess)
      .WillRepeatedly(Return(specialized_features::FeatureAccessFailureSet{}));

  ASSERT_TRUE(scanner_controller->CanShowUi());

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kCanShowUiReturnedTrueWithConsent, 1);
}

TEST_F(ScannerControllerTest,
       CannotShowConsentScreenEntryPointsIfOtherCheckFail) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kConsentNotAccepted,
          specialized_features::FeatureAccessFailure::
              kFeatureManagementCheckFailed}));

  EXPECT_FALSE(scanner_controller->CanShowFeatureSettingsToggle());
}

TEST_F(ScannerControllerTest,
       CanShowFeatureSettingsToggleIfDisabledInSettingsOnly) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kDisabledInSettings}));

  EXPECT_TRUE(scanner_controller->CanShowFeatureSettingsToggle());
}

TEST_F(ScannerControllerTest,
       CanShowFeatureSettingsToggleIfConsentNotAcceptedOnly) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kConsentNotAccepted}));

  EXPECT_TRUE(scanner_controller->CanShowFeatureSettingsToggle());
}

TEST_F(ScannerControllerTest,
       CanShowFeatureSettingsToggleIfDisallowedByPolicy) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(ScannerEnterprisePolicy::kDisallowed));

  EXPECT_TRUE(scanner_controller->CanShowFeatureSettingsToggle());
}

TEST_F(ScannerControllerTest,
       CannotShowFeatureSettingsToggleIfDisabledIfOtherCheckFail) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ON_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
          CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kDisabledInSettings,
          specialized_features::FeatureAccessFailure::
              kFeatureManagementCheckFailed}));

  EXPECT_FALSE(scanner_controller->CanShowFeatureSettingsToggle());
}

TEST_F(ScannerControllerTest, FetchesActionsDuringActiveSession) {
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  output->add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event title");
  EXPECT_CALL(*GetFakeScannerProfileScopedDelegate(*scanner_controller),
              FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(std::move(output), manta::MantaStatus()));

  EXPECT_TRUE(scanner_controller->FetchActionsForImage(
      /*jpeg_bytes=*/nullptr, actions_future.GetCallback()));

  EXPECT_THAT(actions_future.Take(), ValueIs(SizeIs(1)));
}

TEST_F(ScannerControllerTest, NoActionsFetchedWhenNoActiveSession) {
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);

  EXPECT_FALSE(scanner_controller->FetchActionsForImage(
      /*jpeg_bytes=*/nullptr, actions_future.GetCallback()));

  EXPECT_THAT(actions_future.Take(), ValueIs(IsEmpty()));
}

TEST_F(ScannerControllerTest, ResetsScannerSessionWhenActiveUserChanges) {
  SimulateUserLogin({"user1@gmail.com"});
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  EXPECT_TRUE(scanner_controller->HasActiveSessionForTesting());

  // Switch to a different user.
  SimulateUserLogin({"user2@gmail.com"});

  EXPECT_FALSE(scanner_controller->HasActiveSessionForTesting());
}

TEST_F(ScannerControllerTest, ShowsNotificationWhileExecutingNewEventAction) {
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event 1");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_action_details_future;
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(WithArg<2>(InvokeFuture(fetch_action_details_future)));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  // Notification should be shown while action is executing.
  EXPECT_THAT(message_center::MessageCenter::Get()->GetVisibleNotifications(),
              SizeIs(1));

  // Finish executing the action.
  fetch_action_details_future.Take().Run(
      std::make_unique<manta::proto::ScannerOutput>(output),
      manta::MantaStatus());

  // Notification should be hidden.
  EXPECT_THAT(message_center::MessageCenter::Get()->GetVisibleNotifications(),
              IsEmpty());
}

TEST_F(ScannerControllerTest,
       ShowsNotificationWhileExecutingCopyToClipboardAction) {
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()
      ->add_actions()
      ->mutable_copy_to_clipboard()
      ->set_html_text("<b>Hello</b>");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  base::test::TestFuture<manta::ScannerProvider::ScannerProtoResponseCallback>
      fetch_action_details_future;
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(WithArg<2>(InvokeFuture(fetch_action_details_future)));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  // Notification should be shown while the action is executing.
  EXPECT_THAT(message_center::MessageCenter::Get()->GetVisibleNotifications(),
              SizeIs(1));

  // Finish executing the action.
  fetch_action_details_future.Take().Run(
      std::make_unique<manta::proto::ScannerOutput>(output),
      manta::MantaStatus());

  // Notification should be hidden.
  EXPECT_THAT(message_center::MessageCenter::Get()->GetVisibleNotifications(),
              IsEmpty());
}

TEST_F(ScannerControllerTest, ShowsToastAfterCopyToClipboardActionSuccess) {
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()
      ->add_actions()
      ->mutable_copy_to_clipboard()
      ->set_html_text("<b>Hello</b>");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  // Mock a successful action.
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_TRUE(ToastManager::Get()->IsToastShown(kScannerActionSuccessToastId));
}

TEST_F(ScannerControllerTest, DoesNotShowToastAfterNewEventActionSuccess) {
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event title");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  // Mock a successful action.
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_FALSE(ToastManager::Get()->IsToastShown(kScannerActionSuccessToastId));
}

TEST_F(ScannerControllerTest, ShowsToastAfterActionFailure) {
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  auto output = std::make_unique<manta::proto::ScannerOutput>();
  output->add_objects()->add_actions()->mutable_new_event()->set_title(
      "Event title");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(std::move(output), manta::MantaStatus()));
  // Mock a failure to fetch action details.
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          /*output=*/nullptr,
          manta::MantaStatus{.status_code =
                                 manta::MantaStatusCode::kBackendFailure}));

  // Fetch an action and try to execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_TRUE(ToastManager::Get()->IsToastShown(kScannerActionFailureToastId));
}

TEST_F(ScannerControllerTest, ActionSuccessToastButtonOpensFeedbackDialog) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kScannerFeedbackToast);
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()
      ->add_actions()
      ->mutable_copy_to_clipboard()
      ->set_html_text("<b>Hello</b>");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  // Mock a successful action.
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_TRUE(ToastManager::Get()->IsToastShown(kScannerActionSuccessToastId));
  ToastOverlay* toast_overlay =
      Shell::Get()->toast_manager()->GetCurrentOverlayForTesting();
  ASSERT_TRUE(toast_overlay);
  views::Button* feedback_button = toast_overlay->button_for_testing();
  ASSERT_TRUE(feedback_button);

  FakeScannerDelegate& fake_scanner_delegate =
      *GetFakeScannerDelegate(*scanner_controller);
  base::test::TestFuture<const AccountId&, ScannerFeedbackInfo,
                         ScannerDelegate::SendFeedbackCallback>
      feedback_info_future;
  fake_scanner_delegate.SetOpenFeedbackDialogCallback(
      feedback_info_future.GetRepeatingCallback());

  LeftClickOn(feedback_button);

  auto [unused_account_id, feedback_dialog_info,
        unused_send_feedback_callback] = feedback_info_future.Take();
  EXPECT_EQ(feedback_dialog_info.action_details,
            "copy_to_clipboard.html_text: <b>Hello</b>\n");
}

TEST_F(ScannerControllerTest, ActionSuccessToastButtonEmitsMetric) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kScannerFeedbackToast);
  base::HistogramTester histogram_tester;
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()
      ->add_actions()
      ->mutable_copy_to_clipboard()
      ->set_html_text("<b>Hello</b>");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  // Mock a successful action.
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  ASSERT_TRUE(ToastManager::Get()->IsToastShown(kScannerActionSuccessToastId));
  ToastOverlay* toast_overlay =
      Shell::Get()->toast_manager()->GetCurrentOverlayForTesting();
  ASSERT_TRUE(toast_overlay);
  views::Button* feedback_button = toast_overlay->button_for_testing();
  ASSERT_TRUE(feedback_button);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kFeedbackFormOpened, 0);
  LeftClickOn(feedback_button);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      ScannerFeatureUserState::kFeedbackFormOpened, 1);
}

TEST_F(
    ScannerControllerTest,
    ActionSuccessToastDoesNotHaveButtonIfPolicyAllowedWithoutModelImprovement) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kScannerFeedbackToast);
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed,
      static_cast<int>(
          ScannerEnterprisePolicy::kAllowedWithoutModelImprovement));
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()
      ->add_actions()
      ->mutable_copy_to_clipboard()
      ->set_html_text("<b>Hello</b>");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  // Mock a successful action.
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_TRUE(ToastManager::Get()->IsToastShown(kScannerActionSuccessToastId));
  ToastOverlay* overlay =
      Shell::Get()->toast_manager()->GetCurrentOverlayForTesting();
  ASSERT_TRUE(overlay);
  views::Button* button = overlay->button_for_testing();
  EXPECT_FALSE(button);
}

TEST_F(ScannerControllerTest,
       ActionSuccessToastDoesNotHaveButtonIfPolicyInvalidValue) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kScannerFeedbackToast);
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kScannerEnterprisePolicyAllowed, 3);
  base::test::TestFuture<ScannerSession::FetchActionsResponse> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());
  manta::proto::ScannerOutput output;
  output.add_objects()
      ->add_actions()
      ->mutable_copy_to_clipboard()
      ->set_html_text("<b>Hello</b>");
  FakeScannerProfileScopedDelegate& fake_profile_scoped_delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  // Mock a successful action.
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus()));
  EXPECT_CALL(fake_profile_scoped_delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::make_unique<manta::proto::ScannerOutput>(output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  // Fetch an action and execute it.
  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());
  ScannerSession::FetchActionsResponse actions = actions_future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_TRUE(ToastManager::Get()->IsToastShown(kScannerActionSuccessToastId));
  ToastOverlay* overlay =
      Shell::Get()->toast_manager()->GetCurrentOverlayForTesting();
  ASSERT_TRUE(overlay);
  views::Button* button = overlay->button_for_testing();
  EXPECT_FALSE(button);
}

TEST_F(ScannerControllerTest, OpenFeedbackDialogCallsDelegate) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  FakeScannerDelegate& fake_scanner_delegate =
      *GetFakeScannerDelegate(*scanner_controller);
  base::test::TestFuture<const AccountId&, ScannerFeedbackInfo,
                         ScannerDelegate::SendFeedbackCallback>
      feedback_info_future;
  fake_scanner_delegate.SetOpenFeedbackDialogCallback(
      feedback_info_future.GetRepeatingCallback());
  manta::proto::ScannerAction action;
  manta::proto::NewEventAction& new_event = *action.mutable_new_event();
  new_event.set_title("🌏");
  new_event.set_description("formerly \"Geo Sync\"");
  new_event.set_dates("20241014T160000/20241014T161500");
  new_event.set_location("Wonderland");
  auto image = base::MakeRefCounted<base::RefCountedString>("testimage");
  AccountId active_account =
      Shell::Get()->session_controller()->GetActiveAccountId();

  scanner_controller->OpenFeedbackDialog(active_account, std::move(action),
                                         std::move(image));

  auto [account_id, feedback_dialog_info, unused_send_feedback_callback] =
      feedback_info_future.Take();
  EXPECT_EQ(account_id, active_account);
  EXPECT_EQ(feedback_dialog_info.action_details,
            R"(new_event.dates: 20241014T160000/20241014T161500
new_event.description: formerly "Geo Sync"
new_event.location: Wonderland
new_event.title: 🌏
)");
  ASSERT_TRUE(feedback_dialog_info.screenshot);
  EXPECT_EQ(base::as_string_view(*feedback_dialog_info.screenshot),
            "testimage");
}

TEST_F(ScannerControllerTest, OpenFeedbackDialogCallbackSendsFeedback) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  FakeScannerDelegate& fake_scanner_delegate =
      *GetFakeScannerDelegate(*scanner_controller);
  base::test::TestFuture<const AccountId&, ScannerFeedbackInfo,
                         ScannerDelegate::SendFeedbackCallback>
      feedback_info_future;
  fake_scanner_delegate.SetOpenFeedbackDialogCallback(
      feedback_info_future.GetRepeatingCallback());
  base::test::TestFuture<std::string> description_future;
  AccountId active_account =
      Shell::Get()->session_controller()->GetActiveAccountId();
  EXPECT_CALL(mock_send_specialized_feature_feedback(),
              Run(/*account_id=*/active_account,
                  /*product_id=*/feedback::kScannerFeedbackProductId,
                  /*description=*/_,
                  /*image=*/Optional(Eq("testimage")),
                  /*image_mime_type=*/Optional(Eq("image/jpeg"))))
      .WillOnce(
          DoAll(WithArg<2>(InvokeFuture(description_future)), Return(true)));
  manta::proto::ScannerAction action;
  manta::proto::NewEventAction& new_event = *action.mutable_new_event();
  new_event.set_title("🌏");
  new_event.set_description("formerly \"Geo Sync\"");
  new_event.set_dates("20241014T160000/20241014T161500");
  new_event.set_location("Wonderland");
  auto image = base::MakeRefCounted<base::RefCountedString>("testimage");

  scanner_controller->OpenFeedbackDialog(active_account, std::move(action),
                                         std::move(image));
  auto [unused_account_id, feedback_dialog_info, send_feedback_callback] =
      feedback_info_future.Take();
  std::move(send_feedback_callback)
      .Run(std::move(feedback_dialog_info), "user description");

  std::string description = description_future.Take();
  std::vector<std::string_view> split_description =
      base::SplitStringPieceUsingSubstr(
          description,
          "\nuser_description:", base::WhitespaceHandling::KEEP_WHITESPACE,
          base::SplitResult::SPLIT_WANT_ALL);
  constexpr std::string_view kPrefix = "details:  ";
  ASSERT_THAT(split_description,
              ElementsAre(StartsWith(kPrefix), "  user description\n"));
  std::string_view details = split_description[0];
  details.remove_prefix(kPrefix.size());
  EXPECT_THAT(details, IsJson(R"json({
    "new_event": {
      "title": "🌏",
      "description": "formerly \"Geo Sync\"",
      "dates": "20241014T160000/20241014T161500",
      "location": "Wonderland",
    }
  })json"));
}

TEST_F(ScannerControllerTest,
       OpenFeedbackDialogSendFeedbackCallbackSendsMetric) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  FakeScannerDelegate& fake_scanner_delegate =
      *GetFakeScannerDelegate(*scanner_controller);
  base::test::TestFuture<const AccountId&, ScannerFeedbackInfo,
                         ScannerDelegate::SendFeedbackCallback>
      feedback_info_future;
  fake_scanner_delegate.SetOpenFeedbackDialogCallback(
      feedback_info_future.GetRepeatingCallback());
  EXPECT_CALL(mock_send_specialized_feature_feedback(), Run);
  manta::proto::ScannerAction action;
  action.mutable_copy_to_clipboard()->set_html_text("<b>Hello</b>");

  scanner_controller->OpenFeedbackDialog(
      Shell::Get()->session_controller()->GetActiveAccountId(),
      std::move(action),
      /*screenshot=*/base::MakeRefCounted<base::RefCountedString>("testimage"));
  auto [unused_account_id, feedback_dialog_info, send_feedback_callback] =
      feedback_info_future.Take();
  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     ScannerFeatureUserState::kFeedbackSent, 0);
  std::move(send_feedback_callback)
      .Run(std::move(feedback_dialog_info), "feedback");

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     ScannerFeatureUserState::kFeedbackSent, 1);
}

TEST_F(ScannerControllerTest,
       OpenFeedbackDialogCallbackSendsFeedbackWithOriginalAccount) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  FakeScannerDelegate& fake_scanner_delegate =
      *GetFakeScannerDelegate(*scanner_controller);
  base::test::TestFuture<const AccountId&, ScannerFeedbackInfo,
                         ScannerDelegate::SendFeedbackCallback>
      feedback_info_future;
  fake_scanner_delegate.SetOpenFeedbackDialogCallback(
      feedback_info_future.GetRepeatingCallback());
  AccountId original_account =
      Shell::Get()->session_controller()->GetActiveAccountId();
  EXPECT_CALL(mock_send_specialized_feature_feedback(),
              Run(/*account_id=*/original_account,
                  /*product_id=*/_,
                  /*description=*/_,
                  /*image=*/_,
                  /*image_mime_type=*/_))
      .WillOnce(Return(true));

  scanner_controller->OpenFeedbackDialog(
      original_account, manta::proto::ScannerAction(),
      base::MakeRefCounted<base::RefCountedString>());
  auto [account_id, feedback_dialog_info, send_feedback_callback] =
      feedback_info_future.Take();
  ASSERT_EQ(Shell::Get()->session_controller()->GetActiveAccountId(),
            original_account);
  AccountId new_account =
      AccountId::FromUserEmailGaiaId("user@test.com", GaiaId("fakegaia"));
  SimulateUserLogin(new_account);
  ASSERT_NE(Shell::Get()->session_controller()->GetActiveAccountId(),
            original_account);
  std::move(send_feedback_callback)
      .Run(std::move(feedback_dialog_info), "user description");
}

TEST_F(ScannerControllerTest, RunningActionFailsIfActionDetailsFails) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ScannerSession* session = scanner_controller->StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(nullptr, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));

  base::test::TestFuture<bool> action_finished_future;
  scanner_controller->SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_FALSE(action_finished_future.Get());
}

TEST_F(ScannerControllerTest,
       RunningActionFailsIfActionDetailsHaveMultipleObjects) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ScannerSession* session = scanner_controller->StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  auto output_with_multiple_objects =
      std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(output_with_multiple_objects),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(nullptr, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  base::test::TestFuture<bool> action_finished_future;
  scanner_controller->SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_FALSE(action_finished_future.Get());
}

TEST_F(ScannerControllerTest,
       RunningActionFailsIfActionDetailsHaveMultipleActions) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ScannerSession* session = scanner_controller->StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  auto output_with_multiple_actions =
      std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& object = *unpopulated_output->add_objects();
  object.add_actions()->mutable_new_event();
  object.add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(output_with_multiple_actions),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(nullptr, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  base::test::TestFuture<bool> action_finished_future;
  scanner_controller->SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_FALSE(action_finished_future.Get());
}

TEST_F(ScannerControllerTest,
       RunningActionFailsIfActionDetailsHaveDifferentActionCase) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ScannerSession* session = scanner_controller->StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  auto output_with_different_action =
      std::make_unique<manta::proto::ScannerOutput>();
  manta::proto::ScannerObject& object = *unpopulated_output->add_objects();
  object.add_actions()->mutable_new_contact();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(output_with_different_action),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(nullptr, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  base::test::TestFuture<bool> action_finished_future;
  scanner_controller->SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_FALSE(action_finished_future.Get());
}

TEST_F(ScannerControllerTest,
       RunningActionsCallsFetchActionDetailsForImageWithResizedImage) {
  scoped_refptr<base::RefCountedMemory> jpeg_bytes =
      MakeJpegBytes(/*width=*/2300, /*height=*/23000);
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ScannerSession* session = scanner_controller->StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage(
                            Pointee(ResultOf(
                                "decoding JPEG",
                                [](const base::RefCountedMemory& bytes) {
                                  return gfx::JPEGCodec::Decode(bytes);
                                },
                                AllOf(Property(&SkBitmap::width, 230),
                                      Property(&SkBitmap::height, 2300)))),
                            _, _))
      .WillOnce(RunOnceCallback<2>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(jpeg_bytes, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  base::test::TestFuture<bool> action_finished_future;
  scanner_controller->SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller->ExecuteAction(actions.value()[0]);
  ASSERT_TRUE(action_finished_future.IsReady());
}

TEST_F(ScannerControllerTest,
       RunningActionsCallsFetchActionDetailsForImageWithUnpopulatedAction) {
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ScannerSession* session = scanner_controller->StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  manta::proto::ScannerAction unpopulated_action;
  unpopulated_action.mutable_new_event()->set_title("Unpopulated event");
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  *unpopulated_output->add_objects()->add_actions() = unpopulated_action;
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate,
              FetchActionDetailsForImage(_, EqualsProto(unpopulated_action), _))
      .WillOnce(RunOnceCallback<2>(
          nullptr, manta::MantaStatus{
                       .status_code = manta::MantaStatusCode::kInvalidInput}));

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(nullptr, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  base::test::TestFuture<bool> action_finished_future;
  scanner_controller->SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller->ExecuteAction(actions.value()[0]);
  ASSERT_TRUE(action_finished_future.IsReady());
}

TEST_F(ScannerControllerTest, RunningNewEventActionOpensUrl) {
  EXPECT_CALL(mock_new_window_delegate(),
              OpenUrl(Property("spec", &GURL::spec,
                               "https://calendar.google.com/calendar/render"
                               "?action=TEMPLATE"
                               "&text=%F0%9F%8C%8F"
                               "&details=formerly+%22Geo+Sync%22"
                               "&dates=20241014T160000%2F20241014T161500"
                               "&location=Wonderland"),
                      _, _))
      .Times(1);
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  ScannerSession* session = scanner_controller->StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(*scanner_controller);
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_event();
  manta::proto::NewEventAction event_action;
  event_action.set_title("🌏");
  event_action.set_description("formerly \"Geo Sync\"");
  event_action.set_dates("20241014T160000/20241014T161500");
  event_action.set_location("Wonderland");
  auto populated_output = std::make_unique<manta::proto::ScannerOutput>();
  *populated_output->add_objects()->add_actions()->mutable_new_event() =
      std::move(event_action);
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(populated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(nullptr, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  base::test::TestFuture<bool> action_finished_future;
  scanner_controller->SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller->ExecuteAction(actions.value()[0]);

  EXPECT_TRUE(action_finished_future.Get());
}

// This test cannot use AshTestBase as it requires an IO thread.
TEST(ScannerControllerNoFixtureTest, RunningNewContactActionOpensUrl) {
  // ScannerController dependencies:
  // A task environment with an IO thread to use `TestSharedURLLoaderFactory`.
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  // A session controller for `ScopedSessionObserver`.
  SessionControllerImpl session_controller;
  // A message center for showing notifications.
  message_center::MessageCenter::Initialize();
  // A toast manager for showing toasts.
  MockToastManager toast_manager;
  // A new window delegate for opening the URL in test.
  MockNewWindowDelegate mock_new_window_delegate;
  EXPECT_CALL(mock_new_window_delegate,
              OpenUrl(Property("spec", &GURL::spec,
                               "https://contacts.google.com/person/c1?edit=1"),
                      _, _))
      .Times(1);
  ScannerController scanner_controller(std::make_unique<FakeScannerDelegate>(),
                                       session_controller,
                                       /*screen_pinning_controller=*/nullptr);
  ScannerSession* session = scanner_controller.StartNewSession();
  ASSERT_TRUE(session);
  FakeScannerProfileScopedDelegate& delegate =
      *GetFakeScannerProfileScopedDelegate(scanner_controller);
  auto unpopulated_output = std::make_unique<manta::proto::ScannerOutput>();
  unpopulated_output->add_objects()->add_actions()->mutable_new_contact();
  manta::proto::NewContactAction contact_action;
  contact_action.set_given_name("André");
  contact_action.set_family_name("François");
  contact_action.set_email("afrancois@example.com");
  contact_action.set_phone("+61400000000");
  auto populated_output = std::make_unique<manta::proto::ScannerOutput>();
  *populated_output->add_objects()->add_actions()->mutable_new_contact() =
      std::move(contact_action);
  EXPECT_CALL(delegate, FetchActionsForImage)
      .WillOnce(RunOnceCallback<1>(
          std::move(unpopulated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  EXPECT_CALL(delegate, FetchActionDetailsForImage)
      .WillOnce(RunOnceCallback<2>(
          std::move(populated_output),
          manta::MantaStatus{.status_code = manta::MantaStatusCode::kOk}));
  base::MockCallback<
      net::test_server::EmbeddedTestServer::HandleRequestCallback>
      request_callback;
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content(R"json({"resourceName": "people/c1"})json");
  response->set_content_type("application/json");
  EXPECT_CALL(request_callback, Run).WillOnce(Return(std::move(response)));
  delegate.SetRequestCallback(request_callback.Get());

  base::test::TestFuture<ScannerSession::FetchActionsResponse> future;
  session->FetchActionsForImage(nullptr, future.GetCallback());
  ScannerSession::FetchActionsResponse actions = future.Take();
  ASSERT_THAT(actions, ValueIs(SizeIs(1)));
  base::test::TestFuture<bool> action_finished_future;
  scanner_controller.SetOnActionFinishedForTesting(
      action_finished_future.GetCallback());
  scanner_controller.ExecuteAction(actions.value()[0]);

  EXPECT_TRUE(action_finished_future.Get());

  message_center::MessageCenter::Shutdown();
}

}  // namespace

}  // namespace ash
