// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_observers/os_update_event_observer.h"

#include <cstddef>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/core/policy_pref_names.h"
#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/reporting/util/status.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::WithArgs;

namespace {

constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";
static const AccountId kTestAccountId = AccountId::FromUserEmailGaiaId(
    kTestUserEmail,
    signin::GetTestGaiaIdForEmail(kTestUserEmail));

class MockLogUploader : public policy::EventBasedLogUploader {
 public:
  MockLogUploader() = default;
  ~MockLogUploader() override = default;

  MOCK_METHOD(void,
              UploadEventBasedLogs,
              (std::set<support_tool::DataCollectorType>,
               ash::reporting::TriggerEventType,
               std::optional<std::string>,
               policy::EventBasedLogUploader::UploadCallback),
              (override));
};

class OsUpdateEventObserverBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 protected:
  OsUpdateEventObserverBrowserTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kReportOsUpdateStatus, true);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kSystemLogUploadEnabled, true);
  }

  void SetUpOnMainThread() override {
    login_manager_mixin_.SetShouldLaunchBrowser(true);
    PrefService* local_state = g_browser_process->local_state();
    static_cast<PrefRegistrySimple*>(local_state->DeprecatedGetPrefRegistry())
        ->RegisterDictionaryPref(policy::prefs::kEventBasedLogLastUploadTimes);
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();

    // Set up affiliation for the test user.
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();

    device_policy_update->policy_data()->add_device_affiliation_ids(
        kTestAffiliationId);
    user_policy_update->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
  }

  void TearDownOnMainThread() override {
    fake_update_engine_client_ = nullptr;
    policy::DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

  void SendFakeUpdateFailure() {
    update_engine::StatusResult status;
    status.set_new_version("1235.0.0");
    status.set_is_enterprise_rollback(false);
    status.set_will_powerwash_after_reboot(false);
    status.set_current_operation(update_engine::Operation::ERROR);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  ash::FakeSessionManagerClient* session_manager_client();

  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_, kTestAccountId};

  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};

  ash::LoginManagerMixin login_manager_mixin_{
      &mixin_host_, ash::LoginManagerMixin::UserList(), &fake_gaia_mixin_};

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  raw_ptr<ash::FakeUpdateEngineClient> fake_update_engine_client_ = nullptr;
};

}  // namespace

using MockLogUploaderStrict = testing::StrictMock<MockLogUploader>;

IN_PROC_BROWSER_TEST_F(OsUpdateEventObserverBrowserTest,
                       UploadLogsWhenEventObserved) {
  std::unique_ptr<MockLogUploaderStrict> mock_uploader =
      std::make_unique<MockLogUploaderStrict>();
  EXPECT_CALL(*mock_uploader,
              UploadEventBasedLogs(
                  _, ash::reporting::TriggerEventType::OS_UPDATE_FAILED, _, _))
      .WillOnce(WithArgs<2, 3>([](std::optional<std::string> upload_id,
                                  policy::EventBasedLogUploader::UploadCallback
                                      on_upload_completed) {
        // The triggered upload must have an upload ID attached.
        EXPECT_TRUE(upload_id.has_value());
        EXPECT_FALSE(upload_id.value().empty());
        std::move(on_upload_completed).Run(reporting::Status::StatusOK());
      }));

  policy::OsUpdateEventObserver event_observer;
  event_observer.SetLogUploaderForTesting(std::move(mock_uploader));

  SendFakeUpdateFailure();
}
