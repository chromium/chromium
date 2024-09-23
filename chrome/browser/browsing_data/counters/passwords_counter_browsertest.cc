// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/passwords_counter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"

namespace {

using browsing_data::BrowsingDataCounter;
using password_manager::PasswordForm;

class PasswordsCounterTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    finished_ = false;
    time_ = base::Time::Now();
    times_used_in_html_form_ = 0;
    store_ = ProfilePasswordStoreFactory::GetForProfile(
                 browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                 .get();
    SetPasswordsDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void TearDownOnMainThread() override { store_ = nullptr; }

  void AddLogin(const std::string& origin,
                const std::string& username,
                bool blocked_by_user) {
    // Add login and wait until the password store actually changes.
    // on the database thread.
    store_->AddLogin(CreateCredentials(origin, username, blocked_by_user));
    // GetLogins() blocks until reading on the background thread is finished.
    passwords_helper::GetLogins(store_);
  }

  void RemoveLogin(const std::string& origin,
                   const std::string& username,
                   bool blocked_by_user) {
    // Remove login and wait until the password store actually changes
    // on the database thread.
    store_->RemoveLogin(FROM_HERE,
                        CreateCredentials(origin, username, blocked_by_user));
    // GetLogins() blocks until reading on the background thread is finished.
    passwords_helper::GetLogins(store_);
  }

  void SetPasswordsDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeletePasswords, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void RevertTimeInDays(int days) { time_ -= base::Days(days); }

  void SetTimesUsed(int occurrences) { times_used_in_html_form_ = occurrences; }

  void WaitForCounting() {
    // The counting takes place on the background thread. Wait until it
    // finishes. GetLogins() blocks until reading on the background thread is
    // finished
    passwords_helper::GetLogins(store_);

    // At this point, the calculation on DB thread should have finished, and
    // a callback should be scheduled on the UI thread. Process the tasks until
    // we get a finished result.
    if (finished_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // Once the GetResult() or GetDomainExamples()  is called, it can be called
  // again until the next result is available.
  BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    // Some tests call WaitForCounting() multiple times. Set `finished_` to
    // false such that next call of WaitForCounting() will indeed block until
    // counting is done.
    finished_ = false;
    return result_;
  }

  // Once the GetResult() or GetDomainExamples() is called, it can be called
  // again until the next result is available.
  std::vector<std::string> GetDomainExamples() {
    DCHECK(finished_);
    // Some tests call WaitForCounting() multiple times. Set `finished_` to
    // false such that next call of WaitForCounting() will indeed block until
    // counting is done.
    finished_ = false;
    return domain_examples_;
  }

  void Callback(std::unique_ptr<BrowsingDataCounter::Result> result) {
    DCHECK(result);
    finished_ = result->Finished();

    if (finished_) {
      auto* password_result =
          static_cast<browsing_data::PasswordsCounter::PasswordsResult*>(
              result.get());
      result_ = password_result->Value();
      domain_examples_ = password_result->domain_examples();
    }
    if (run_loop_ && finished_) {
      run_loop_->Quit();
    }
  }

 private:
  PasswordForm CreateCredentials(const std::string& origin,
                                 const std::string& username,
                                 bool blocked_by_user) {
    PasswordForm result;
    result.signon_realm = origin;
    result.url = GURL(origin);
    if (!blocked_by_user) {
      result.username_value = base::ASCIIToUTF16(username);
      result.password_value = u"hunter2";
    }
    result.blocked_by_user = blocked_by_user;
    result.date_created = time_;
    result.times_used_in_html_form = times_used_in_html_form_;
    return result;
  }

  raw_ptr<password_manager::PasswordStoreInterface> store_ = nullptr;

  std::unique_ptr<base::RunLoop> run_loop_;
  base::Time time_;
  int times_used_in_html_form_;

  bool finished_;
  BrowsingDataCounter::ResultInt result_;
  std::vector<std::string> domain_examples_;
};

// Tests that the counter correctly counts each individual credential on
// the same domain.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, SameDomain) {
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", false);
  AddLogin("https://www.google.com", "user3", false);
  AddLogin("https://www.chrome.com", "user1", false);
  AddLogin("https://www.chrome.com", "user2", false);

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&PasswordsCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(5u, GetResult());
}

// Tests that the counter doesn't count blocklisted entries.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, blocklisted) {
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "", true);
  AddLogin("https://www.chrome.com", "", true);

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));

  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&PasswordsCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(1u, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, PrefChanged) {
  SetPasswordsDeletionPref(false);
  AddLogin("https://www.google.com", "user", false);
  AddLogin("https://www.chrome.com", "user", false);

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&PasswordsCounterTest::Callback,
                                   base::Unretained(this)));
  SetPasswordsDeletionPref(true);

  WaitForCounting();
  EXPECT_EQ(2u, GetResult());
}

// Tests that the counter starts counting automatically when
// the password store changes.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, StoreChanged) {
  AddLogin("https://www.google.com", "user", false);

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&PasswordsCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  AddLogin("https://www.chrome.com", "user", false);
  WaitForCounting();
  EXPECT_EQ(2u, GetResult());

  RemoveLogin("https://www.chrome.com", "user", false);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());
}

// Tests that changing the deletion period restarts the counting, and that
// the result takes login creation dates into account.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, PeriodChanged) {
  AddLogin("https://www.google.com", "user", false);
  RevertTimeInDays(2);
  AddLogin("https://example.com", "user1", false);
  RevertTimeInDays(3);
  AddLogin("https://example.com", "user2", false);
  RevertTimeInDays(30);
  AddLogin("https://www.chrome.com", "user", false);

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&PasswordsCounterTest::Callback,
                                   base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_WEEK);
  WaitForCounting();
  EXPECT_EQ(3u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::FOUR_WEEKS);
  WaitForCounting();
  EXPECT_EQ(3u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::OLDER_THAN_30_DAYS);
  WaitForCounting();
  EXPECT_EQ(1u, GetResult());

  SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  WaitForCounting();
  EXPECT_EQ(4u, GetResult());
}

// Tests that the two most commonly used domains are chosen as the listed domain
// examples.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, MostCommonDomains) {
  SetTimesUsed(3);
  AddLogin("https://www.google.com", "user", false);
  SetTimesUsed(4);
  AddLogin("https://wwww.maps.google.com", "user", false);
  SetTimesUsed(1);
  AddLogin("https://www.example.com", "user", false);
  SetTimesUsed(2);
  AddLogin("https://www.chrome.com", "user", false);

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&PasswordsCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  std::vector<std::string> domain_examples = {"google.com", "chrome.com"};
  EXPECT_EQ(domain_examples, GetDomainExamples());
}

// Tests that the counter doesn't crash if restarted in a quick succession.
// TODO(crbug.com/40918960): Upgrade this test to use SigninDataCounter.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, MultipleRestarts) {
  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile));
  counter.Init(profile->GetPrefs(),
               browsing_data::ClearBrowsingDataTab::ADVANCED,
               base::BindRepeating(&PasswordsCounterTest::Callback,
                                   base::Unretained(this)));
  counter.Restart();
  counter.Restart();
  counter.Restart();

  // Previous restarts should be invalidated, so we only need to wait for
  // counting once.
  WaitForCounting();
}

}  // namespace
