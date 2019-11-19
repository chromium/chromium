// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/policy/affiliation_test_helper.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/net/nss_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/auth_policy/fake_auth_policy_client.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
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

constexpr char kAffiliatedUser[] = "affiliated-user@example.com";
constexpr char kAffiliatedUserGaiaId[] = "1234567890";
constexpr char kAffiliatedUserObjGuid[] =
    "{11111111-1111-1111-1111-111111111111}";
constexpr char kAffiliationID[] = "some-affiliation-id";
constexpr char kAnotherAffiliationID[] = "another-affiliation-id";

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

void CheckIsSystemSlotAvailableOnIOThread(
    content::ResourceContext* resource_context,
    bool* out_system_slot_available,
    base::OnceClosure done_closure) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto did_get_cert_db_callback = base::BindRepeating(
      &CheckIsSystemSlotAvailableOnIOThreadWithCertDb,
      out_system_slot_available,
      base::AdaptCallbackForRepeating(std::move(done_closure)));

  net::NSSCertDatabase* cert_db = GetNSSCertDatabaseForResourceContext(
      resource_context, did_get_cert_db_callback);
  if (cert_db)
    did_get_cert_db_callback.Run(cert_db);
}

// Returns true if the system token is available for |profile|. System token
// availability is one of the aspects which are tied to user affiliation. It is
// an interesting one to test because it is evaluated very early (in
// ProfileIOData::InitializeOnUIThread).
bool IsSystemSlotAvailable(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop run_loop;
  bool system_slot_available = false;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(CheckIsSystemSlotAvailableOnIOThread,
                     profile->GetResourceContext(), &system_slot_available,
                     run_loop.QuitClosure()));
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

    if (GetParam().active_directory) {
      account_id_ = AccountId::AdFromUserEmailObjGuid(kAffiliatedUser,
                                                      kAffiliatedUserObjGuid);
    } else {
      account_id_ = AccountId::FromUserEmailGaiaId(kAffiliatedUser,
                                                   kAffiliatedUserGaiaId);
    }
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
          cryptohome::CreateAccountIdentifierFromAccountId(account_id_);
      command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                      cryptohome_id.account_id());
      command_line->AppendSwitchASCII(
          chromeos::switches::kLoginProfile,
          chromeos::CryptohomeClient::GetStubSanitizedUsername(cryptohome_id));
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Initialize clients here so they are available during setup. They will be
    // shutdown in ChromeBrowserMain.
    chromeos::SessionManagerClient::InitializeFakeInMemory();
    chromeos::UpstartClient::InitializeFake();
    chromeos::FakeAuthPolicyClient* fake_auth_policy_client = nullptr;
    if (GetParam().active_directory) {
      chromeos::AuthPolicyClient::InitializeFake();
      fake_auth_policy_client = chromeos::FakeAuthPolicyClient::Get();
      fake_auth_policy_client->DisableOperationDelayForTesting();
    }

    DevicePolicyCrosTestHelper test_helper;
    UserPolicyBuilder user_policy;
    const std::set<std::string> device_affiliation_ids = {kAffiliationID};
    const std::set<std::string> user_affiliation_ids = {
        GetParam().affiliated ? kAffiliationID : kAnotherAffiliationID};

    auto* session_manager_client = chromeos::FakeSessionManagerClient::Get();
    AffiliationTestHelper affiliation_helper =
        GetParam().active_directory
            ? AffiliationTestHelper::CreateForActiveDirectory(
                  session_manager_client, fake_auth_policy_client)
            : AffiliationTestHelper::CreateForCloud(session_manager_client);

    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetDeviceAffiliationIDs(
        &test_helper, device_affiliation_ids));

    ASSERT_NO_FATAL_FAILURE(affiliation_helper.SetUserAffiliationIDs(
        &user_policy, account_id_, user_affiliation_ids));

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

  AccountId account_id_;

  void SetUpTestSystemSlot() {
    bool system_slot_constructed_successfully = false;
    base::RunLoop loop;
    base::PostTaskAndReply(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&UserAffiliationBrowserTest::SetUpTestSystemSlotOnIO,
                       base::Unretained(this),
                       &system_slot_constructed_successfully),
        loop.QuitClosure());
    loop.Run();
    ASSERT_TRUE(system_slot_constructed_successfully);
  }

  void VerifyAffiliationExpectations() {
    EXPECT_EQ(GetParam().affiliated, user_manager::UserManager::Get()
                                         ->FindUser(account_id_)
                                         ->IsAffiliated());

    // Also test system slot availability, which is tied to user affiliation.
    // This gives us additional information, because for the system slot to be
    // available for an affiliated user, IsAffiliated() must already be
    // returning true in the ProfileIOData constructor.
    ASSERT_NO_FATAL_FAILURE(SetUpTestSystemSlot());
    EXPECT_EQ(GetParam().affiliated,
              IsSystemSlotAvailable(ProfileManager::GetPrimaryUserProfile()));
  }

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
    base::PostTaskAndReply(
        FROM_HERE, {content::BrowserThread::IO},
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
  AffiliationTestHelper::PreLoginUser(account_id_);
}

// This part of the test performs a regular sign-in through the login manager.
IN_PROC_BROWSER_TEST_P(UserAffiliationBrowserTest, PRE_TestAffiliation) {
  AffiliationTestHelper::LoginUser(account_id_);
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
