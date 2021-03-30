// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_context.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/cert/nss_cert_database.h"

namespace {

void NotCalledDbCallback(net::NSSCertDatabase* db) { ASSERT_TRUE(false); }

// DBTester handles retrieving the NSSCertDatabase for a given profile, and
// doing some simple sanity checks.
// Browser test cases run on the UI thread, while the nss_context access needs
// to happen on the IO thread. The DBTester class encapsulates the thread
// posting and waiting on the UI thread so that the test case body can be
// written linearly.
class DBTester {
 public:
  explicit DBTester(Profile* profile) : profile_(profile), db_(NULL) {}

  // Initial retrieval of cert database. It may be asynchronous or synchronous.
  // Returns true if the database was retrieved successfully.
  bool DoGetDBTests() {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DBTester::GetDBAndDoTestsOnIOThread,
                                  base::Unretained(this),
                                  CreateNSSCertDatabaseGetter(profile_),
                                  run_loop.QuitClosure()));
    run_loop.Run();
    return !!db_;
  }

  // Test retrieving the database again, should be called after DoGetDBTests.
  void DoGetDBAgainTests() {
    base::RunLoop run_loop;
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&DBTester::DoGetDBAgainTestsOnIOThread,
                                  base::Unretained(this),
                                  CreateNSSCertDatabaseGetter(profile_),
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
    db_ = db;
    EXPECT_TRUE(db);
    if (db) {
      EXPECT_TRUE(db->GetPublicSlot().get());
      // Public and private slot are the same in tests.
      EXPECT_EQ(db->GetPublicSlot().get(), db->GetPrivateSlot().get());
    }

    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(done_callback));
  }

  void DoGetDBAgainTestsOnIOThread(NssCertDatabaseGetter database_getter,
                                   base::OnceClosure done_callback) {
    net::NSSCertDatabase* db =
        std::move(database_getter).Run(base::BindOnce(&NotCalledDbCallback));
    // Should always be synchronous now.
    EXPECT_TRUE(db);
    // Should return the same db as before.
    EXPECT_EQ(db_, db);

    content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                 std::move(done_callback));
  }

  Profile* profile_;
  net::NSSCertDatabase* db_;
};

class UserAddingFinishObserver : public chromeos::UserAddingScreen::Observer {
 public:
  UserAddingFinishObserver() {
    chromeos::UserAddingScreen::Get()->AddObserver(this);
  }

  ~UserAddingFinishObserver() override {
    chromeos::UserAddingScreen::Get()->RemoveObserver(this);
  }

  void WaitUntilUserAddingFinishedOrCancelled() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    if (finished_)
      return;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  // chromeos::UserAddingScreen::Observer:
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
  DISALLOW_COPY_AND_ASSIGN(UserAddingFinishObserver);
};

}  // namespace

class NSSContextChromeOSBrowserTest : public chromeos::LoginManagerTest {
 public:
  NSSContextChromeOSBrowserTest() : LoginManagerTest() {
    login_mixin_.AppendRegularUsers(2);
  }
  ~NSSContextChromeOSBrowserTest() override {}

 protected:
  chromeos::LoginManagerMixin login_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(NSSContextChromeOSBrowserTest, TwoUsers) {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();

  // Log in first user and get their DB.
  const AccountId account_id1(login_mixin_.users()[0].account_id);
  LoginUser(account_id1);
  Profile* profile1 = chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(
      user_manager->FindUser(account_id1));
  ASSERT_TRUE(profile1);

  DBTester tester1(profile1);
  ASSERT_TRUE(tester1.DoGetDBTests());

  // Log in second user and get their DB.
  UserAddingFinishObserver observer;
  chromeos::UserAddingScreen::Get()->Start();
  base::RunLoop().RunUntilIdle();

  const AccountId account_id2(login_mixin_.users()[1].account_id);
  AddUser(account_id2);
  observer.WaitUntilUserAddingFinishedOrCancelled();

  Profile* profile2 = chromeos::ProfileHelper::Get()->GetProfileByUserUnsafe(
      user_manager->FindUser(account_id2));
  ASSERT_TRUE(profile2);

  DBTester tester2(profile2);
  ASSERT_TRUE(tester2.DoGetDBTests());

  // Get both DBs again to check that the same object is returned.
  tester1.DoGetDBAgainTests();
  tester2.DoGetDBAgainTests();

  // Check that each user has a separate DB and NSS slots.
  tester1.DoNotEqualsTests(&tester2);
}
