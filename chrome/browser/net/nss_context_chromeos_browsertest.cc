// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/user_adding_screen.h"
#include "chrome/test/base/ash/scoped_test_system_nss_key_slot_mixin.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/cert/nss_cert_database.h"

namespace {

constexpr char kTestEmail1[] = "test1@example.com";
constexpr char kTestEmail2[] = "test2@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";

void NotCalledDbCallback(net::NSSCertDatabase* db) {
  ASSERT_TRUE(false);
}

// DBTester handles retrieving the NSSCertDatabase for a given profile, and
// doing some simple sanity checks.
// Browser test cases run on the UI thread, while the nss_context access needs
// to happen on the IO thread. The DBTester class encapsulates the thread
// posting and waiting on the UI thread so that the test case body can be
// written linearly.
class DBTester {
 public:
  explicit DBTester(Profile* profile, bool will_have_system_slot)
      : profile_(profile),
        db_(nullptr),
        will_have_system_slot_(will_have_system_slot) {}

  // Initial retrieval of cert database. It may be asynchronous or synchronous.
  // Returns true if the database was retrieved successfully.
  bool DoGetDBTests() {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DBTester::GetDBAndDoTestsOnIOThread,
                       base::Unretained(this),
                       NssServiceFactory::GetForContext(profile_)
                           ->CreateNSSCertDatabaseGetterForIOThread(),
                       run_loop.QuitClosure()));
    run_loop.Run();
    return !!db_;
  }

  // Test retrieving the database again, should be called after DoGetDBTests.
  void DoGetDBAgainTests() {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DBTester::DoGetDBAgainTestsOnIOThread,
                       base::Unretained(this),
                       NssServiceFactory::GetForContext(profile_)
                           ->CreateNSSCertDatabaseGetterForIOThread(),
                       run_loop.QuitClosure()));
    run_loop.Run();
  }

  void DoNotEqualsTests(DBTester* other_tester) {
    // The DB and its NSS slots should be different for each profile.
    EXPECT_NE(db_, other_tester->db_);
    EXPECT_NE(db_->GetPublicSlot().get(),
              other_tester->db_->GetPublicSlot().get());
  }

 private:
  void GetDBAndDoTestsOnIOThread(NssCertDatabaseGetter database_getter,
                                 const base::RepeatingClosure& done_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    net::NSSCertDatabase* db =
        std::move(database_getter)
            .Run(base::BindOnce(&DBTester::DoTestsOnIOThread,
                                base::Unretained(this), done_callback));
    if (db) {
      DVLOG(1) << "got db synchronously";
      DoTestsOnIOThread(done_callback, db);
    } else {
      DVLOG(1) << "getting db asynchronously...";
    }
  }

  void DoTestsOnIOThread(base::OnceClosure done_callback,
                         net::NSSCertDatabase* db) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    db_ = db;
    EXPECT_TRUE(db);
    if (db) {
      EXPECT_TRUE(db->GetPublicSlot().get());
      // Public and private slot are the same in tests.
      EXPECT_EQ(db->GetPublicSlot().get(), db->GetPrivateSlot().get());
      // System slot should be already initialized when the database is
      // available.
      EXPECT_EQ(will_have_system_slot_, !!db->GetSystemSlot());
    }

    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(done_callback));
  }

  void DoGetDBAgainTestsOnIOThread(NssCertDatabaseGetter database_getter,
                                   base::OnceClosure done_callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    net::NSSCertDatabase* db =
        std::move(database_getter).Run(base::BindOnce(&NotCalledDbCallback));
    // Should always be synchronous now.
    EXPECT_TRUE(db);
    // Should return the same db as before.
    EXPECT_EQ(db_, db);

    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(done_callback));
  }

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<net::NSSCertDatabase> db_ = nullptr;
  // Indicates if the tester should expect to receive a database with
  // initialized system slot or not.
  bool will_have_system_slot_ = false;
};

class UserAddingFinishObserver : public ash::UserAddingScreen::Observer {
 public:
  UserAddingFinishObserver() {
    ash::UserAddingScreen::Get()->AddObserver(this);
  }

  UserAddingFinishObserver(const UserAddingFinishObserver&) = delete;
  UserAddingFinishObserver& operator=(const UserAddingFinishObserver&) = delete;

  ~UserAddingFinishObserver() override {
    ash::UserAddingScreen::Get()->RemoveObserver(this);
  }

  void WaitUntilUserAddingFinishedOrCancelled() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (finished_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // ash::UserAddingScreen::Observer:
  void OnUserAddingFinished() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    finished_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void OnBeforeUserAddingScreenStarted() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    finished_ = false;
  }

 private:
  bool finished_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class NSSContextChromeOSBrowserTest : public ash::LoginManagerTest {
 public:
  NSSContextChromeOSBrowserTest() : LoginManagerTest() {
    // These users will be unaffiliated and will have indexes after the
    // affiliated ones in the `login_mixin_.users()` array.
    login_mixin_.AppendRegularUsers(2);
  }
  ~NSSContextChromeOSBrowserTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    auto device_policy_update = device_state_mixin_.RequestDevicePolicyUpdate();
    auto user_policy_update_1 = user_policy_mixin_1_.RequestPolicyUpdate();
    auto user_policy_update_2 = user_policy_mixin_2_.RequestPolicyUpdate();

    device_policy_update->policy_data()->add_device_affiliation_ids(
        kTestAffiliationId);
    user_policy_update_1->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
    user_policy_update_2->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
  }

 protected:
  // Affiliated user 1
  AccountId affiliated_account_id_1_{AccountId::FromUserEmailGaiaId(
      kTestEmail1,
      signin::GetTestGaiaIdForEmail(kTestEmail1))};
  ash::LoginManagerMixin::TestUserInfo affiliated_user_1_{
      affiliated_account_id_1_};
  ash::UserPolicyMixin user_policy_mixin_1_{&mixin_host_,
                                            affiliated_account_id_1_};

  // Affiliated user 2
  AccountId affiliated_account_id_2_{AccountId::FromUserEmailGaiaId(
      kTestEmail2,
      signin::GetTestGaiaIdForEmail(kTestEmail2))};
  ash::LoginManagerMixin::TestUserInfo affiliated_user_2_{
      affiliated_account_id_2_};
  ash::UserPolicyMixin user_policy_mixin_2_{&mixin_host_,
                                            affiliated_account_id_2_};

  // Indexes of unaffiliated users in the `login_mixin_.users()` array.` The
  // affiliated users above will take indexes 0 and 1.
  static constexpr size_t kUnaffiliatedUserIdx1 = 2;
  static constexpr size_t kUnaffiliatedUserIdx2 = 3;

  ash::ScopedTestSystemNSSKeySlotMixin system_nss_key_slot_mixin_{&mixin_host_};
  ash::DeviceStateMixin device_state_mixin_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  ash::LoginManagerMixin login_mixin_{
      &mixin_host_,
      /*initial_users=*/{affiliated_user_1_, affiliated_user_2_}};
};

IN_PROC_BROWSER_TEST_F(NSSContextChromeOSBrowserTest,
                       AffiliatedUserHasSystemSlot) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  LoginUser(affiliated_account_id_1_);
  Profile* profile1 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(affiliated_account_id_1_));
  ASSERT_TRUE(profile1);

  DBTester tester1(profile1, /*will_have_system_slot=*/true);
  ASSERT_TRUE(tester1.DoGetDBTests());

  tester1.DoGetDBAgainTests();
}

IN_PROC_BROWSER_TEST_F(NSSContextChromeOSBrowserTest,
                       UnaffiliatedUserDoesNotHaveSystemSlot) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  const AccountId account_id1(
      login_mixin_.users()[kUnaffiliatedUserIdx1].account_id);
  LoginUser(account_id1);
  Profile* profile1 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id1));
  ASSERT_TRUE(profile1);

  DBTester tester1(profile1, /*will_have_system_slot=*/false);
  ASSERT_TRUE(tester1.DoGetDBTests());

  tester1.DoGetDBAgainTests();
}

IN_PROC_BROWSER_TEST_F(NSSContextChromeOSBrowserTest,
                       DISABLED_TwoAffiliatedUsersHaveSystemSlots) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Log in first user and get their DB.
  LoginUser(affiliated_account_id_1_);
  Profile* profile1 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(affiliated_account_id_1_));
  ASSERT_TRUE(profile1);

  DBTester tester1(profile1, /*will_have_system_slot=*/true);
  ASSERT_TRUE(tester1.DoGetDBTests());

  // Log in second user and get their DB.
  UserAddingFinishObserver observer;
  ash::UserAddingScreen::Get()->Start();
  base::RunLoop().RunUntilIdle();

  AddUser(affiliated_account_id_2_);
  observer.WaitUntilUserAddingFinishedOrCancelled();

  Profile* profile2 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(affiliated_account_id_2_));
  ASSERT_TRUE(profile2);

  DBTester tester2(profile2, /*will_have_system_slot=*/true);
  ASSERT_TRUE(tester2.DoGetDBTests());

  // Get both DBs again to check that the same object is returned.
  tester1.DoGetDBAgainTests();
  tester2.DoGetDBAgainTests();

  // Check that each user has a separate DB and NSS slots.
  tester1.DoNotEqualsTests(&tester2);
}

IN_PROC_BROWSER_TEST_F(NSSContextChromeOSBrowserTest,
                       TwoUnaffiliatedUsersDontHaveSystemSlots) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Log in first user and get their DB.
  const AccountId account_id1(
      login_mixin_.users()[kUnaffiliatedUserIdx1].account_id);
  LoginUser(account_id1);
  Profile* profile1 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id1));
  ASSERT_TRUE(profile1);

  DBTester tester1(profile1, /*will_have_system_slot=*/false);
  ASSERT_TRUE(tester1.DoGetDBTests());

  // Log in second user and get their DB.
  UserAddingFinishObserver observer;
  ash::UserAddingScreen::Get()->Start();
  base::RunLoop().RunUntilIdle();

  const AccountId account_id2(
      login_mixin_.users()[kUnaffiliatedUserIdx2].account_id);
  AddUser(account_id2);
  observer.WaitUntilUserAddingFinishedOrCancelled();

  Profile* profile2 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id2));
  ASSERT_TRUE(profile2);

  DBTester tester2(profile2, /*will_have_system_slot=*/false);
  ASSERT_TRUE(tester2.DoGetDBTests());

  // Get both DBs again to check that the same object is returned.
  tester1.DoGetDBAgainTests();
  tester2.DoGetDBAgainTests();

  // Check that each user has a separate DB and NSS slots.
  tester1.DoNotEqualsTests(&tester2);
}

IN_PROC_BROWSER_TEST_F(NSSContextChromeOSBrowserTest,
                       TwoUsersOnlyAffiliatedHasSystemSlot) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Log in first user and get their DB.
  LoginUser(affiliated_account_id_1_);
  Profile* profile1 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(affiliated_account_id_1_));
  ASSERT_TRUE(profile1);

  DBTester tester1(profile1, /*will_have_system_slot=*/true);
  ASSERT_TRUE(tester1.DoGetDBTests());

  // Log in second user and get their DB.
  UserAddingFinishObserver observer;
  ash::UserAddingScreen::Get()->Start();
  base::RunLoop().RunUntilIdle();

  const AccountId account_id2(
      login_mixin_.users()[kUnaffiliatedUserIdx1].account_id);
  AddUser(account_id2);
  observer.WaitUntilUserAddingFinishedOrCancelled();

  Profile* profile2 = ash::ProfileHelper::Get()->GetProfileByUser(
      user_manager->FindUser(account_id2));
  ASSERT_TRUE(profile2);

  DBTester tester2(profile2, /*will_have_system_slot=*/false);
  ASSERT_TRUE(tester2.DoGetDBTests());

  // Get both DBs again to check that the same object is returned.
  tester1.DoGetDBAgainTests();
  tester2.DoGetDBAgainTests();

  // Check that each user has a separate DB and NSS slots.
  tester1.DoNotEqualsTests(&tester2);
}
