// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/passwords_private/test_passwords_private_delegate.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/time_format.h"

namespace extensions {

namespace {

class PasswordsPrivateApiTest : public ExtensionApiTest {
 public:
  PasswordsPrivateApiTest() = default;
  ~PasswordsPrivateApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    s_test_delegate_ = static_cast<TestPasswordsPrivateDelegate*>(
        PasswordsPrivateDelegateFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating([](content::BrowserContext* context) {
              return std::unique_ptr<KeyedService>(
                  new TestPasswordsPrivateDelegate());
            })));
    s_test_delegate_->SetProfile(profile());
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunPasswordsSubtest(const std::string& subtest) {
    const std::string page_url = "main.html?" + subtest;
    return RunExtensionTest({.name = "passwords_private",
                             .page_url = page_url.c_str(),
                             .load_as_component = true});
  }

  bool importPasswordsWasTriggered() {
    return s_test_delegate_->ImportPasswordsTriggered();
  }

  bool exportPasswordsWasTriggered() {
    return s_test_delegate_->ExportPasswordsTriggered();
  }

  bool cancelExportPasswordsWasTriggered() {
    return s_test_delegate_->CancelExportPasswordsTriggered();
  }

  bool start_password_check_triggered() {
    return s_test_delegate_->StartPasswordCheckTriggered();
  }

  bool stop_password_check_triggered() {
    return s_test_delegate_->StopPasswordCheckTriggered();
  }

  void set_start_password_check_state(
      password_manager::BulkLeakCheckService::State state) {
    s_test_delegate_->SetStartPasswordCheckState(state);
  }

  bool IsOptedInForAccountStorage() {
    return s_test_delegate_->IsOptedInForAccountStorage();
  }

  void SetOptedInForAccountStorage(bool opted_in) {
    s_test_delegate_->SetAccountStorageOptIn(opted_in, nullptr);
  }

  void ResetPlaintextPassword() { s_test_delegate_->ResetPlaintextPassword(); }

  void AddCompromisedCredential(int id) {
    s_test_delegate_->AddCompromisedCredential(id);
  }

  const std::vector<int>& last_moved_passwords() const {
    return s_test_delegate_->last_moved_passwords();
  }

 private:
  TestPasswordsPrivateDelegate* s_test_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PasswordsPrivateApiTest);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, ChangeSavedPasswordSucceeds) {
  EXPECT_TRUE(RunPasswordsSubtest("changeSavedPasswordSucceeds")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeSavedPasswordWithIncorrectIdFails) {
  EXPECT_TRUE(RunPasswordsSubtest("changeSavedPasswordWithIncorrectIdFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeSavedPasswordWithOneIncorrectIdFromArrayFails) {
  EXPECT_TRUE(RunPasswordsSubtest(
      "changeSavedPasswordWithOneIncorrectIdFromArrayFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeSavedPasswordWithEmptyPasswordFails) {
  EXPECT_TRUE(RunPasswordsSubtest("changeSavedPasswordWithEmptyPasswordFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeSavedPasswordWithEmptyArrayIdFails) {
  EXPECT_TRUE(RunPasswordsSubtest("changeSavedPasswordWithEmptyArrayIdFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveAndUndoRemoveSavedPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("removeAndUndoRemoveSavedPassword"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveAndUndoRemoveSavedPasswordsBatch) {
  EXPECT_TRUE(RunPasswordsSubtest("removeAndUndoRemoveSavedPasswordsBatch"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveAndUndoRemovePasswordException) {
  EXPECT_TRUE(RunPasswordsSubtest("removeAndUndoRemovePasswordException"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveAndUndoRemovePasswordExceptionsBatch) {
  EXPECT_TRUE(RunPasswordsSubtest("removeAndUndoRemovePasswordExceptionsBatch"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestPlaintextPassword) {
  EXPECT_TRUE(RunPasswordsSubtest("requestPlaintextPassword")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestPlaintextPasswordFails) {
  ResetPlaintextPassword();
  EXPECT_TRUE(RunPasswordsSubtest("requestPlaintextPasswordFails")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetSavedPasswordList) {
  EXPECT_TRUE(RunPasswordsSubtest("getSavedPasswordList")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetPasswordExceptionList) {
  EXPECT_TRUE(RunPasswordsSubtest("getPasswordExceptionList")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, ImportPasswords) {
  EXPECT_FALSE(importPasswordsWasTriggered());
  EXPECT_TRUE(RunPasswordsSubtest("importPasswords")) << message_;
  EXPECT_TRUE(importPasswordsWasTriggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, ExportPasswords) {
  EXPECT_FALSE(exportPasswordsWasTriggered());
  EXPECT_TRUE(RunPasswordsSubtest("exportPasswords")) << message_;
  EXPECT_TRUE(exportPasswordsWasTriggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, CancelExportPasswords) {
  EXPECT_FALSE(cancelExportPasswordsWasTriggered());
  EXPECT_TRUE(RunPasswordsSubtest("cancelExportPasswords")) << message_;
  EXPECT_TRUE(cancelExportPasswordsWasTriggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RequestExportProgressStatus) {
  EXPECT_TRUE(RunPasswordsSubtest("requestExportProgressStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, IsNotOptedInForAccountStorage) {
  EXPECT_TRUE(RunPasswordsSubtest("isNotOptedInForAccountStorage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, IsOptedInForAccountStorage) {
  SetOptedInForAccountStorage(true);
  EXPECT_TRUE(RunPasswordsSubtest("isOptedInForAccountStorage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetCompromisedCredentials) {
  EXPECT_TRUE(RunPasswordsSubtest("getCompromisedCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetWeakCredentials) {
  EXPECT_TRUE(RunPasswordsSubtest("getWeakCredentials")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetPlaintextInsecurePassword) {
  EXPECT_TRUE(RunPasswordsSubtest("getPlaintextInsecurePassword")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       GetPlaintextInsecurePasswordFails) {
  ResetPlaintextPassword();
  EXPECT_TRUE(RunPasswordsSubtest("getPlaintextInsecurePasswordFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeInsecureCredentialWithEmptyPasswordFails) {
  EXPECT_TRUE(
      RunPasswordsSubtest("changeInsecureCredentialWithEmptyPasswordFails"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, ChangeInsecureCredentialFails) {
  EXPECT_TRUE(RunPasswordsSubtest("changeInsecureCredentialFails")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       ChangeInsecureCredentialSucceeds) {
  AddCompromisedCredential(0);
  EXPECT_TRUE(RunPasswordsSubtest("changeInsecureCredentialSucceeds"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, OptInForAccountStorage) {
  SetOptedInForAccountStorage(false);
  EXPECT_TRUE(RunPasswordsSubtest("optInForAccountStorage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, OptOutForAccountStorage) {
  SetOptedInForAccountStorage(true);
  EXPECT_TRUE(RunPasswordsSubtest("optOutForAccountStorage")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, RemoveInsecureCredentialFails) {
  EXPECT_TRUE(RunPasswordsSubtest("removeInsecureCredentialFails")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest,
                       RemoveInsecureCredentialSucceeds) {
  AddCompromisedCredential(0);
  EXPECT_TRUE(RunPasswordsSubtest("removeInsecureCredentialSucceeds"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, StartPasswordCheck) {
  set_start_password_check_state(
      password_manager::BulkLeakCheckService::State::kRunning);
  EXPECT_FALSE(start_password_check_triggered());
  EXPECT_TRUE(RunPasswordsSubtest("startPasswordCheck")) << message_;
  EXPECT_TRUE(start_password_check_triggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, StartPasswordCheckFailed) {
  set_start_password_check_state(
      password_manager::BulkLeakCheckService::State::kIdle);
  EXPECT_FALSE(start_password_check_triggered());
  EXPECT_TRUE(RunPasswordsSubtest("startPasswordCheckFailed")) << message_;
  EXPECT_TRUE(start_password_check_triggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, StopPasswordCheck) {
  EXPECT_FALSE(stop_password_check_triggered());
  EXPECT_TRUE(RunPasswordsSubtest("stopPasswordCheck")) << message_;
  EXPECT_TRUE(stop_password_check_triggered());
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, GetPasswordCheckStatus) {
  EXPECT_TRUE(RunPasswordsSubtest("getPasswordCheckStatus")) << message_;
}

IN_PROC_BROWSER_TEST_F(PasswordsPrivateApiTest, MovePasswordsToAccount) {
  EXPECT_TRUE(last_moved_passwords().empty());
  EXPECT_TRUE(RunPasswordsSubtest("movePasswordsToAccount")) << message_;
  EXPECT_EQ(42, last_moved_passwords()[0]);
}

}  // namespace extensions
