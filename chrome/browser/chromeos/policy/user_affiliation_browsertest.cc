// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>

#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/affiliation_mixin.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/authpolicy/fake_authpolicy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "crypto/scoped_test_system_nss_key_slot.h"
#include "net/cert/nss_cert_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class ResourceContext;
}

namespace policy {

namespace {

struct Params {
  Params(bool affiliated, bool active_directory)
      : affiliated(affiliated), active_directory(active_directory) {}

  // Whether the user is expected to be affiliated.
  bool affiliated;

  // Whether the user account is an Active Directory account.
  bool active_directory;
};

// Must be a valid test name (no spaces etc.). Makes the test show up as e.g.
// AffiliationCheck/U.A.B.T.Affiliated/NotAffiliated_NotActiveDirectory
std::string PrintParam(testing::TestParamInfo<Params> param_info) {
  return base::StringPrintf("%sAffiliated_%sActiveDirectory",
                            param_info.param.affiliated ? "" : "Not",
                            param_info.param.active_directory ? "" : "Not");
}

void CheckIsSystemSlotAvailableOnIOThreadWithCertDb(
    bool* out_system_slot_available,
    base::OnceClosure done_closure,
    net::NSSCertDatabase* cert_db) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  *out_system_slot_available = cert_db->GetSystemSlot() != nullptr;
  std::move(done_closure).Run();
}

void CheckIsSystemSlotAvailableOnIOThread(NssCertDatabaseGetter database_getter,
                                          bool* out_system_slot_available,
                                          base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto did_get_cert_db_split_callback = base::SplitOnceCallback(
      base::BindOnce(&CheckIsSystemSlotAvailableOnIOThreadWithCertDb,
                     out_system_slot_available, std::move(done_closure)));

  net::NSSCertDatabase* cert_db =
      std::move(database_getter)
          .Run(std::move(did_get_cert_db_split_callback.first));
  if (cert_db) {
    std::move(did_get_cert_db_split_callback.second).Run(cert_db);
  }
}

// Returns true if the system token is available for |profile|. System token
// availability is one of the aspects which are tied to user affiliation. It is
// an interesting one to test because it is evaluated very early (in
// ProfileIOData::InitializeOnUIThread).
bool IsSystemSlotAvailable(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  bool system_slot_available = false;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(CheckIsSystemSlotAvailableOnIOThread,
                     CreateNSSCertDatabaseGetter(profile),
                     &system_slot_available, run_loop.QuitClosure()));
  run_loop.Run();
  return system_slot_available;
}

}  // namespace

class UserAffiliationBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<Params> {
 public:
  UserAffiliationBrowserTest() {
    set_exit_when_last_browser_closes(false);
    affiliation_mixin_.SetIsForActiveDirectory(GetParam().active_directory);
    affiliation_mixin_.set_affiliated(GetParam().affiliated);
  }

 protected:
  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    if (content::IsPreTest()) {
      AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
          command_line);
    } else {
      const cryptohome::AccountIdentifier cryptohome_id =
          cryptohome::CreateAccountIdentifierFromAccountId(
              affiliation_mixin_.account_id());
      command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                      cryptohome_id.account_id());
      command_line->AppendSwitchASCII(
          chromeos::switches::kLoginProfile,
          chromeos::UserDataAuthClient::GetStubSanitizedUsername(
              cryptohome_id));
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Initialize clients here so they are available during setup. They will be
    // shutdown in ChromeBrowserMain.
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::UpstartClient::InitializeFake();
    if (GetParam().active_directory) {
      chromeos::AuthPolicyClient::InitializeFake();
      chromeos::FakeAuthPolicyClient::Get()->DisableOperationDelayForTesting();
    }

    // Set retry delay to prevent timeouts.
    policy::DeviceManagementService::SetRetryDelayForTesting(0);
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    MixinBasedInProcessBrowserTest::CreatedBrowserMainParts(browser_main_parts);

    login_ui_visible_waiter_ =
        std::make_unique<content::WindowedNotificationObserver>(
            chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
            content::NotificationService::AllSources());
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    if (content::IsPreTest()) {
      // Wait for the login manager UI to be available before continuing.
      // This is a workaround for chrome crashing when running with DCHECKS when
      // it exits while the login manager is being loaded.
      // TODO(pmarko): Remove this when https://crbug.com/869272 is fixed.
      login_ui_visible_waiter_->Wait();
    }
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();

    TearDownTestSystemSlot();
  }

  void SetUpTestSystemSlot() {
    bool system_slot_constructed_successfully = false;
    base::RunLoop loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&UserAffiliationBrowserTest::SetUpTestSystemSlotOnIO,
                       base::Unretained(this),
                       &system_slot_constructed_successfully),
        loop.QuitClosure());
    loop.Run();
    ASSERT_TRUE(system_slot_constructed_successfully);
  }

  void VerifyAffiliationExpectations() {
    EXPECT_EQ(GetParam().affiliated,
              user_manager::UserManager::Get()
                  ->FindUser(affiliation_mixin_.account_id())
                  ->IsAffiliated());

    // Also test system slot availability, which is tied to user affiliation.
    // This gives us additional information, because for the system slot to be
    // available for an affiliated user, IsAffiliated() must already be
    // returning true in the ProfileIOData constructor.
    ASSERT_NO_FATAL_FAILURE(SetUpTestSystemSlot());
    EXPECT_EQ(GetParam().affiliated,
              IsSystemSlotAvailable(ProfileManager::GetPrimaryUserProfile()));
  }

  DevicePolicyCrosTestHelper test_helper_;
  AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};

 private:
  void SetUpTestSystemSlotOnIO(bool* out_system_slot_constructed_successfully) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    test_system_slot_ = std::make_unique<crypto::ScopedTestSystemNSSKeySlot>();
    *out_system_slot_constructed_successfully =
        test_system_slot_->ConstructedSuccessfully();
  }

  void TearDownTestSystemSlot() {
    if (!test_system_slot_)
      return;

    base::RunLoop loop;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&UserAffiliationBrowserTest::TearDownTestSystemSlotOnIO,
                       base::Unretained(this)),
        loop.QuitClosure());
    loop.Run();
  }

  void TearDownTestSystemSlotOnIO() { test_system_slot_.reset(); }

  std::unique_ptr<crypto::ScopedTestSystemNSSKeySlot> test_system_slot_;

  std::unique_ptr<content::WindowedNotificationObserver>
      login_ui_visible_waiter_;

  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      GetParam().active_directory
          ? chromeos::DeviceStateMixin::State::
                OOBE_COMPLETED_ACTIVE_DIRECTORY_ENROLLED
          : chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  DISALLOW_COPY_AND_ASSIGN(UserAffiliationBrowserTest);
};

IN_PROC_BROWSER_TEST_P(UserAffiliationBrowserTest, PRE_PRE_TestAffiliation) {
  AffiliationTestHelper::PreLoginUser(affiliation_mixin_.account_id());
}

// This part of the test performs a regular sign-in through the login manager.
IN_PROC_BROWSER_TEST_P(UserAffiliationBrowserTest, PRE_TestAffiliation) {
  AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  ASSERT_NO_FATAL_FAILURE(VerifyAffiliationExpectations());
}

// This part of the test performs a direct sign-in into the user profile using
// the --login-user command-line switch, simulating the restart after crash
// behavior on Chrome OS.
// See SetUpCommandLine for details.
IN_PROC_BROWSER_TEST_P(UserAffiliationBrowserTest, TestAffiliation) {
  // Note: We don't log in the user, because the login has implicitly been
  // performed using a command-line flag (see SetUpCommandLine).
  ASSERT_NO_FATAL_FAILURE(VerifyAffiliationExpectations());
}

// TODO(https://crbug.com/946024): PRE_ test is flakily timing out.
INSTANTIATE_TEST_SUITE_P(DISABLED_AffiliationCheck,
                         UserAffiliationBrowserTest,
                         //         affiliated            active_directory
                         //              |                         |
                         //              +----------+      ______  +---------+
                         //                         |     /      \______     |
                         ::testing::Values(Params(true, true),     //   \   /
                                           Params(false, true),    //    \ /
                                           Params(true, false),    //     X
                                           Params(false, false)),  //    / \<--!
                         PrintParam);                              //    \_/

}  // namespace policy
