// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/dbus/arc_crosh_service_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/mojom/crosh.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/connection_holder_util.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/test/scoped_command_line.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/message.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {
namespace {

// Arrows controlling the result of Exec() from Mojo.
class MockArcShellExecutionInstance
    : public arc::mojom::ArcShellExecutionInstance {
 public:
  void Exec(arc::mojom::ArcShellExecutionRequestPtr request,
            ExecCallback callback) override {
    EXPECT_TRUE(!request.is_null());
    recieved_exec_request_ = std::move(request);

    EXPECT_TRUE(!callback.is_null());
    EXPECT_TRUE(!exec_result_.is_null());

    std::move(callback).Run(std::move(exec_result_));
  }

  arc::mojom::ArcShellExecutionRequestPtr RecievedExecRequest() {
    return std::move(recieved_exec_request_);
  }

  void SetNextExecResult(arc::mojom::ArcShellExecutionResultPtr result) {
    exec_result_ = std::move(result);
  }

 private:
  arc::mojom::ArcShellExecutionResultPtr exec_result_;
  arc::mojom::ArcShellExecutionRequestPtr recieved_exec_request_;
};

constexpr char kPrimaryUserProfileName[] = "primary@gmail.com";
constexpr char KSecondaryUserProfileName[] = "secondary@gmail.com";

class ArcCroshServiceProviderTest : public testing::Test {
 public:
  ArcCroshServiceProviderTest() = default;
  ArcCroshServiceProviderTest(const ArcCroshServiceProviderTest&) = delete;
  ArcCroshServiceProviderTest& operator=(const ArcCroshServiceProviderTest&) =
      delete;
  ~ArcCroshServiceProviderTest() override = default;

  void SetUp() override {
    ash::UpstartClient::InitializeFake();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);

    base::CommandLine* command_line =
        scoped_command_line.GetProcessCommandLine();
    command_line->InitFromArgv(
        {"", "--arc-availability=officially-supported", "--enable-arcvm"});

    service_provider_ = std::make_unique<ArcCroshServiceProvider>();
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();

    // Make the session manager skip creating UI.
    arc::ArcSessionManager::SetUiEnabledForTesting(/*enabled=*/false);
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));

    // Log in as a primary profile to enable ARCVM.
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    const AccountId primary_user_account_id =
        AccountId::FromUserEmail(kPrimaryUserProfileName);
    user_manager::User* primary_user =
        fake_user_manager_->AddUser(primary_user_account_id);
    primary_username_hash_ = primary_user->username_hash();
    Profile* primary_user_profile =
        profile_manager_->CreateTestingProfile(kPrimaryUserProfileName);

    const AccountId secondary_user_account_id =
        AccountId::FromUserEmail(KSecondaryUserProfileName);
    user_manager::User* secondary_user =
        fake_user_manager_->AddUser(secondary_user_account_id);
    secondary_username_hash_ = secondary_user->username_hash();
    profile_manager_->CreateTestingProfile(KSecondaryUserProfileName);

    fake_user_manager_->LoginUser(primary_user_account_id);
    arc_session_manager_->SetProfile(primary_user_profile);

    arc_session_manager_->Initialize();
    arc_session_manager_->RequestEnable();
    arc_session_manager_->StartArcForTesting();

    arc_bridge_service()->arc_shell_execution()->SetInstance(
        &mock_arc_shell_execution_instance_);
    arc::WaitForInstanceReady(arc_bridge_service()->arc_shell_execution());

    test_helper_.SetUp(arc::crosh::kArcCroshServiceName,
                       dbus::ObjectPath(arc::crosh::kArcCroshServicePath),
                       arc::crosh::kArcCroshInterfaceName,
                       arc::crosh::kArcCroshRequest, service_provider_.get());
  }

  void TearDown() override {
    test_helper_.TearDown();

    arc_bridge_service()->arc_shell_execution()->CloseInstance(
        &mock_arc_shell_execution_instance_);
    arc_session_manager_->Shutdown();
    profile_manager_->DeleteTestingProfile(kPrimaryUserProfileName);
    profile_manager_->DeleteTestingProfile(KSecondaryUserProfileName);
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    service_provider_.reset();
    ash::ConciergeClient::Shutdown();
    ash::UpstartClient::Shutdown();
  }

 protected:
  std::unique_ptr<dbus::Response> CallExec(
      arc::ArcShellExecutionRequest message) {
    dbus::MethodCall method_call{arc::crosh::kArcCroshServiceName,
                                 arc::crosh::kArcCroshRequest};
    dbus::MessageWriter writer(&method_call);
    writer.AppendProtoAsArrayOfBytes(message);

    std::unique_ptr<dbus::Response> response =
        test_helper_.CallMethod(&method_call);
    return response;
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

  MockArcShellExecutionInstance& mock_arc_shell_execution_instance() {
    return mock_arc_shell_execution_instance_;
  }

  arc::ArcBridgeService* arc_bridge_service() {
    return arc_service_manager_->arc_bridge_service();
  }

  const std::string& primary_username_hash() const {
    return primary_username_hash_;
  }
  const std::string& secondary_username_hash() const {
    return secondary_username_hash_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ServiceProviderTestHelper test_helper_;
  std::unique_ptr<ArcCroshServiceProvider> service_provider_;
  MockArcShellExecutionInstance mock_arc_shell_execution_instance_;
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  session_manager::SessionManager session_manager_;
  std::string primary_username_hash_;
  std::string secondary_username_hash_;
  base::test::ScopedCommandLine scoped_command_line;
};

TEST_F(ArcCroshServiceProviderTest, Success) {
  arc::mojom::ArcShellExecutionResultPtr mock_result =
      arc::mojom::ArcShellExecutionResult::NewStdout("top command result");
  mock_arc_shell_execution_instance().SetNextExecResult(std::move(mock_result));

  arc::ArcShellExecutionRequest request;
  request.set_command(arc::ArcShellExecutionRequest_ArcShellCommand::
                          ArcShellExecutionRequest_ArcShellCommand_TOP);
  request.set_user_id(primary_username_hash());

  std::unique_ptr<dbus::Response> response = CallExec(request);

  arc::mojom::ArcShellExecutionRequestPtr sent_mojo_request =
      mock_arc_shell_execution_instance().RecievedExecRequest();
  EXPECT_EQ(sent_mojo_request->command, arc::mojom::ArcShellCommand::kTop);

  arc::ArcShellExecutionResult result;
  dbus::MessageReader reader(response.get());
  ASSERT_TRUE(reader.PopArrayOfBytesAsProto(&result));

  EXPECT_FALSE(result.has_error());
  ASSERT_TRUE(result.has_stdout());
  EXPECT_EQ("top command result", result.stdout());
}

TEST_F(ArcCroshServiceProviderTest, EmptyCommand) {
  arc::ArcShellExecutionRequest request;
  request.set_user_id(primary_username_hash());

  std::unique_ptr<dbus::Response> response = CallExec(request);

  EXPECT_EQ(DBUS_ERROR_INVALID_ARGS, response->GetErrorName());

  std::string error_message;
  dbus::MessageReader reader(response.get());
  ASSERT_TRUE(reader.PopString(&error_message));

  EXPECT_EQ("command should not be empty", error_message);
}

TEST_F(ArcCroshServiceProviderTest, RequestWithoutUserId) {
  arc::ArcShellExecutionRequest request;
  request.set_command(arc::ArcShellExecutionRequest_ArcShellCommand::
                          ArcShellExecutionRequest_ArcShellCommand_TOP);

  std::unique_ptr<dbus::Response> response = CallExec(request);

  EXPECT_EQ(DBUS_ERROR_INVALID_ARGS, response->GetErrorName());

  std::string error_message;
  dbus::MessageReader reader(response.get());
  ASSERT_TRUE(reader.PopString(&error_message));

  EXPECT_EQ("user_id should not be empty", error_message);
}

TEST_F(ArcCroshServiceProviderTest, RequestFromSecondaryUser) {
  arc::ArcShellExecutionRequest request;
  request.set_command(arc::ArcShellExecutionRequest_ArcShellCommand::
                          ArcShellExecutionRequest_ArcShellCommand_TOP);
  request.set_user_id(secondary_username_hash());

  std::unique_ptr<dbus::Response> response = CallExec(request);

  EXPECT_EQ(DBUS_ERROR_ACCESS_DENIED, response->GetErrorName());

  std::string error_message;
  dbus::MessageReader reader(response.get());
  ASSERT_TRUE(reader.PopString(&error_message));

  EXPECT_EQ("Request from not primary user is prohibited", error_message);
}

}  // namespace
}  // namespace ash
