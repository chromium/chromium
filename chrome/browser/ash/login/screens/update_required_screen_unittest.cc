// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/update_required_screen.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_switches.h"
#include "ash/login/test_login_screen.h"
#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "chrome/browser/ash/login/screens/mock_error_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ui/webui/ash/login/fake_update_required_screen_handler.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/portal_detector/mock_network_portal_detector.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

class UpdateRequiredScreenUnitTest : public testing::Test {
 public:
  UpdateRequiredScreenUnitTest()
      : local_state_(TestingBrowserProcess::GetGlobal()) {}

  UpdateRequiredScreenUnitTest(const UpdateRequiredScreenUnitTest&) = delete;
  UpdateRequiredScreenUnitTest& operator=(const UpdateRequiredScreenUnitTest&) =
      delete;

  void SetUpdateEngineStatus(update_engine::Operation operation) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  // testing::Test:
  void SetUp() override {
    // Initialize objects needed by `UpdateRequiredScreen`.
    wizard_context_ = std::make_unique<WizardContext>();
    fake_view_ = std::make_unique<FakeUpdateRequiredScreenHandler>();
    fake_update_engine_client_ = UpdateEngineClient::InitializeFakeForTest();

    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();

    mock_network_portal_detector_ = new MockNetworkPortalDetector();
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector_);
    mock_error_screen_ =
        std::make_unique<MockErrorScreen>(mock_error_view_.AsWeakPtr());

    // Ensure proper behavior of `UpdateRequiredScreen`'s supporting objects.
    EXPECT_CALL(*mock_network_portal_detector_, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    update_required_screen_ = std::make_unique<UpdateRequiredScreen>(
        fake_view_.get()->AsWeakPtr(), mock_error_screen_.get(),
        base::DoNothing());

    update_required_screen_->GetVersionUpdaterForTesting()
        ->set_wait_for_reboot_time_for_testing(base::Seconds(0));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);

    wizard_context_.reset();
    update_required_screen_.reset();
    mock_error_screen_.reset();

    network_portal_detector::Shutdown();
    network_handler_test_helper_.reset();
    UpdateEngineClient::Shutdown();
  }

 protected:
  // A pointer to the `UpdateRequiredScreen` used in this test.
  std::unique_ptr<UpdateRequiredScreen> update_required_screen_;

  // Accessory objects needed by `UpdateRequiredScreen`.
  TestLoginScreen test_login_screen_;
  std::unique_ptr<FakeUpdateRequiredScreenHandler> fake_view_;
  MockErrorScreenView mock_error_view_;
  std::unique_ptr<MockErrorScreen> mock_error_screen_;
  std::unique_ptr<WizardContext> wizard_context_;
  // Will be deleted in `network_portal_detector::Shutdown()`.
  raw_ptr<MockNetworkPortalDetector, DanglingUntriaged>
      mock_network_portal_detector_;
  // Will be deleted in `DBusThreadManager::Shutdown()`.
  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged> fake_update_engine_client_;
  // Initializes NetworkHandler and required DBus clients.
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;

 private:
  // Test versions of core browser infrastructure.
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  ScopedTestingCrosSettings scoped_testing_cros_settings_;
  // This is used for `GetEnterpriseDisplayDomain`.
  ScopedStubInstallAttributes test_install_attributes_;
};

namespace {
constexpr char kUserActionUpdateButtonClicked[] = "update";
constexpr char kUserActionAcceptUpdateOverCellular[] = "update-accept-cellular";
}  // namespace

TEST_F(UpdateRequiredScreenUnitTest, HandlesNoUpdate) {
  // DUT reaches `UpdateRequiredScreen`.
  update_required_screen_->Show(wizard_context_.get());
  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
  base::Value::List args;
  args.Append(kUserActionUpdateButtonClicked);
  update_required_screen_->HandleUserAction(args);

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  // No updates are available.
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::IDLE);

  EXPECT_EQ(fake_view_->ui_state(), UpdateRequiredView::UPDATE_ERROR);
}

TEST_F(UpdateRequiredScreenUnitTest, HandlesUpdateExists) {
  // DUT reaches `UpdateRequiredScreen`.
  update_required_screen_->Show(wizard_context_.get());
  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
  base::Value::List args;
  args.Append(kUserActionUpdateButtonClicked);
  update_required_screen_->HandleUserAction(args);

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  EXPECT_EQ(fake_view_->ui_state(), UpdateRequiredView::UPDATE_PROCESS);

  SetUpdateEngineStatus(update_engine::Operation::VERIFYING);

  SetUpdateEngineStatus(update_engine::Operation::FINALIZING);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_GE(fake_update_engine_client_->reboot_after_update_call_count(), 1);

  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_COMPLETED_NEED_REBOOT);
}

TEST_F(UpdateRequiredScreenUnitTest, HandlesCellularPermissionNeeded) {
  // DUT reaches `UpdateRequiredScreen`.
  update_required_screen_->Show(wizard_context_.get());
  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_REQUIRED_MESSAGE);
  {
    base::Value::List args;
    args.Append(kUserActionUpdateButtonClicked);
    update_required_screen_->HandleUserAction(args);
  }

  // Verify that the DUT checks for an update.
  EXPECT_EQ(fake_update_engine_client_->request_update_check_call_count(), 1);

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);

  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);

  SetUpdateEngineStatus(update_engine::Operation::NEED_PERMISSION_TO_UPDATE);

  {
    base::Value::List args;
    args.Append(kUserActionAcceptUpdateOverCellular);
    update_required_screen_->HandleUserAction(args);
  }

  EXPECT_GE(
      fake_update_engine_client_->update_over_cellular_permission_count() +
          fake_update_engine_client_
              ->update_over_cellular_one_time_permission_count(),
      1);

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  EXPECT_EQ(fake_view_->ui_state(), UpdateRequiredView::UPDATE_PROCESS);

  SetUpdateEngineStatus(update_engine::Operation::VERIFYING);

  SetUpdateEngineStatus(update_engine::Operation::FINALIZING);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  EXPECT_GE(fake_update_engine_client_->reboot_after_update_call_count(), 1);

  EXPECT_EQ(fake_view_->ui_state(),
            UpdateRequiredView::UPDATE_COMPLETED_NEED_REBOOT);
}

}  // namespace
}  // namespace ash
