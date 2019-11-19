// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/logging.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {
constexpr char kTestUser[] = "test-user@gmail.com";
constexpr char kTestUserGaiaId[] = "1234567890";
constexpr char kLogFileName[] = "chrome.log";
constexpr char kLogMessageBrowser[] = "browser log before logging in";
constexpr char kLogMessageBrowserRedirected[] = "browser log after logging in";
constexpr char kLogMessageNetwork[] = "network service log before logging in";
constexpr char kLogMessageNetworkRedirected[] = "network service log logged in";

base::FilePath GetLogFilePath(const base::ScopedTempDir& dir) {
  return dir.GetPath().Append(kLogFileName);
}

std::string GetLogFileContents(const base::ScopedTempDir& dir) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string log_contents;
  CHECK(base::ReadFileToString(GetLogFilePath(dir), &log_contents));
  return log_contents;
}

void LogToNetworkService(std::string message) {
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterface(
      network_service_test.BindNewPipeAndPassReceiver());
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test->Log(message);
}
}  // namespace

class LoggingBrowserTest : public LoginManagerTest {
 public:
  LoggingBrowserTest() : LoginManagerTest(true, true) {
    CHECK(system_temp_dir_.CreateUniqueTempDir());
    CHECK(user_temp_dir_.CreateUniqueTempDir());
    logging::ForceLogRedirectionForTesting();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    // Enable logging to a file, and configure pre and post login logs to go to
    // different folders.
    command_line->AppendSwitch(::switches::kEnableLogging);
    command_line->AppendSwitchNative(::switches::kLogFile,
                                     GetLogFilePath(system_temp_dir_).value());
    base::Environment::Create()->SetVar(env_vars::kSessionLogDir,
                                        user_temp_dir_.GetPath().value());
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    // This isn't done in SetUpCommandLine because InProcessBrowserTest::SetUp
    // sets kDisableLoggingRedirect after it gets called.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        ::switches::kDisableLoggingRedirect);
  }

 protected:
  base::ScopedTempDir system_temp_dir_;
  base::ScopedTempDir user_temp_dir_;
};

IN_PROC_BROWSER_TEST_F(LoggingBrowserTest, PRE_NetworkServiceLogsRedirect) {
  RegisterUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  EXPECT_EQ(session_manager::SessionState::OOBE,
            session_manager::SessionManager::Get()->session_state());
  chromeos::StartupUtils::MarkOobeCompleted();
}

IN_PROC_BROWSER_TEST_F(LoggingBrowserTest, NetworkServiceLogsRedirect) {
  // Log some messages pre-redirect.
  LOG(ERROR) << kLogMessageBrowser;
  LogToNetworkService(kLogMessageNetwork);

  // Log the user in which will redirect the logs.
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session_manager::SessionManager::Get()->session_state());
  LoginUser(AccountId::FromUserEmailGaiaId(kTestUser, kTestUserGaiaId));
  EXPECT_EQ(session_manager::SessionState::ACTIVE,
            session_manager::SessionManager::Get()->session_state());

  // Log some messages post-redirect.
  LOG(ERROR) << kLogMessageBrowserRedirected;
  LogToNetworkService(kLogMessageNetworkRedirected);

  // Verify the log content.
  std::string system_logs = GetLogFileContents(system_temp_dir_);
  std::string user_logs = GetLogFileContents(user_temp_dir_);
  EXPECT_NE(std::string::npos, system_logs.find(kLogMessageBrowser));
  EXPECT_NE(std::string::npos, system_logs.find(kLogMessageNetwork));
  EXPECT_EQ(std::string::npos, system_logs.find(kLogMessageBrowserRedirected));
  EXPECT_EQ(std::string::npos, system_logs.find(kLogMessageNetworkRedirected));
  EXPECT_EQ(std::string::npos, user_logs.find(kLogMessageBrowser));
  EXPECT_EQ(std::string::npos, user_logs.find(kLogMessageNetwork));
  EXPECT_NE(std::string::npos, user_logs.find(kLogMessageBrowserRedirected));
  EXPECT_NE(std::string::npos, user_logs.find(kLogMessageNetworkRedirected));
}

}  // namespace chromeos
