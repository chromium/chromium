// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/passwords_counter.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

using autofill::PasswordForm;
using browsing_data::BrowsingDataCounter;

class PasswordsCounterTest : public InProcessBrowserTest {
 public:
  PasswordsCounterTest() {}

  void SetUpOnMainThread() override {
    finished_ = false;
    time_ = base::Time::Now();
    times_used_ = 0;
    store_ = PasswordStoreFactory::GetForProfile(
        browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    SetPasswordsDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void AddLogin(const std::string& origin,
                const std::string& username,
                bool blacklisted) {
    // Add login and wait until the password store actually changes.
    // on the database thread.
    passwords_helper::AddLogin(
        store_.get(), CreateCredentials(origin, username, blacklisted));
  }

  void RemoveLogin(const std::string& origin,
                   const std::string& username,
                   bool blacklisted) {
    // Remove login and wait until the password store actually changes
    // on the database thread.
    passwords_helper::RemoveLogin(
        store_.get(), CreateCredentials(origin, username, blacklisted));
  }

  void SetPasswordsDeletionPref(bool value) {
    browser()->profile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeletePasswords, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->profile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  void RevertTimeInDays(int days) {
    time_ -= base::TimeDelta::FromDays(days);
  }

  void SetTimesUsed(int occurrences) { times_used_ = occurrences; }

  void WaitForCounting() {
    // The counting takes place on the database thread. Wait until it finishes.
    base::WaitableEvent waitable_event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    store_->ScheduleTask(base::BindOnce(&base::WaitableEvent::Signal,
                                        base::Unretained(&waitable_event)));
    waitable_event.Wait();

    // At this point, the calculation on DB thread should have finished, and
    // a callback should be scheduled on the UI thread. Process the tasks until
    // we get a finished result.
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  std::vector<std::string> GetDomainExamples() {
    DCHECK(finished_);
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
    if (run_loop_ && finished_)
      run_loop_->Quit();
  }

  void WaitForUICallbacksFromAddingLogins() {
    base::RunLoop loop;
    loop.RunUntilIdle();
  }

 private:
  PasswordForm CreateCredentials(const std::string& origin,
                                 const std::string& username,
                                 bool blacklisted) {
    PasswordForm result;
    result.signon_realm = origin;
    result.origin = GURL(origin);
    result.username_value = base::ASCIIToUTF16(username);
    result.password_value = base::ASCIIToUTF16("hunter2");
    result.blacklisted_by_user = blacklisted;
    result.date_created = time_;
    result.times_used = times_used_;
    return result;
  }

  scoped_refptr<password_manager::PasswordStore> store_;

  std::unique_ptr<base::RunLoop> run_loop_;
  base::Time time_;
  int times_used_;

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
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  EXPECT_EQ(5u, GetResult());
}

// Tests that the counter doesn't count blacklisted entries.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, Blacklisted) {
  AddLogin("https://www.google.com", "user1", false);
  AddLogin("https://www.google.com", "user2", true);
  AddLogin("https://www.chrome.com", "user3", true);
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));

  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
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
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
  SetPasswordsDeletionPref(true);

  WaitForCounting();
  EXPECT_EQ(2u, GetResult());
}

// Tests that the counter starts counting automatically when
// the password store changes.
IN_PROC_BROWSER_TEST_F(PasswordsCounterTest, StoreChanged) {
  AddLogin("https://www.google.com", "user", false);
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
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
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));

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
  WaitForUICallbacksFromAddingLogins();

  Profile* profile = browser()->profile();
  browsing_data::PasswordsCounter counter(
      PasswordStoreFactory::GetForProfile(profile,
                                          ServiceAccessType::EXPLICIT_ACCESS),
      ProfileSyncServiceFactory::GetForProfile(profile));
  counter.Init(
      profile->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&PasswordsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  WaitForCounting();
  std::vector<std::string> domain_examples = {"google.com", "chrome.com"};
  EXPECT_EQ(domain_examples, GetDomainExamples());
}

}  // namespace
