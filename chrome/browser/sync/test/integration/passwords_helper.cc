// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/passwords_helper.h"

#include <sstream>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "content/public/test/test_utils.h"

using autofill::PasswordForm;
using password_manager::PasswordStore;
using sync_datatype_helper::test;

namespace {

const char kFakeSignonRealm[] = "http://fake-signon-realm.google.com/";
const char kIndexedFakeOrigin[] = "http://fake-signon-realm.google.com/%d";

// We use a WaitableEvent to wait when logins are added, removed, or updated
// instead of running the UI message loop because of a restriction that
// prevents a DB thread from initiating a quit of the UI message loop.
void PasswordStoreCallback(base::WaitableEvent* wait_event) {
  // Wake up passwords_helper::AddLogin.
  wait_event->Signal();
}

class PasswordStoreConsumerHelper
    : public password_manager::PasswordStoreConsumer {
 public:
  PasswordStoreConsumerHelper() {}

  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override {
    result_.swap(results);
    run_loop_.Quit();
  }

  std::vector<std::unique_ptr<PasswordForm>> WaitForResult() {
    DCHECK(!run_loop_.running());
    content::RunThisRunLoop(&run_loop_);
    return std::move(result_);
  }

 private:
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<PasswordForm>> result_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreConsumerHelper);
};

// PasswordForm::date_synced is a local field. Therefore it may be different
// across clients.
void ClearSyncDateField(std::vector<std::unique_ptr<PasswordForm>>* forms) {
  for (auto& form : *forms) {
    form->date_synced = base::Time();
  }
}

}  // namespace

namespace passwords_helper {

void AddLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->AddLogin(form);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void UpdateLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->UpdateLogin(form);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

std::vector<std::unique_ptr<PasswordForm>> GetLogins(PasswordStore* store) {
  EXPECT_TRUE(store);
  password_manager::PasswordStore::FormDigest matcher_form = {
      PasswordForm::SCHEME_HTML, kFakeSignonRealm, GURL()};
  PasswordStoreConsumerHelper consumer;
  store->GetLogins(matcher_form, &consumer);
  return consumer.WaitForResult();
}

void RemoveLogin(PasswordStore* store, const PasswordForm& form) {
  ASSERT_TRUE(store);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  store->RemoveLogin(form);
  store->ScheduleTask(base::BindOnce(&PasswordStoreCallback, &wait_event));
  wait_event.Wait();
}

void RemoveLogins(PasswordStore* store) {
  std::vector<std::unique_ptr<PasswordForm>> forms = GetLogins(store);
  for (const auto& form : forms) {
    RemoveLogin(store, *form);
  }
}

PasswordStore* GetPasswordStore(int index) {
  return PasswordStoreFactory::GetForProfile(test()->GetProfile(index),
                                             ServiceAccessType::IMPLICIT_ACCESS)
      .get();
}

PasswordStore* GetVerifierPasswordStore() {
  return PasswordStoreFactory::GetForProfile(
             test()->verifier(), ServiceAccessType::IMPLICIT_ACCESS).get();
}

bool ProfileContainsSamePasswordFormsAsVerifier(int index) {
  std::vector<std::unique_ptr<PasswordForm>> verifier_forms =
      GetLogins(GetVerifierPasswordStore());
  std::vector<std::unique_ptr<PasswordForm>> forms =
      GetLogins(GetPasswordStore(index));
  ClearSyncDateField(&forms);

  std::ostringstream mismatch_details_stream;
  bool is_matching = password_manager::ContainsEqualPasswordFormsUnordered(
      verifier_forms, forms, &mismatch_details_stream);
  if (!is_matching) {
    VLOG(1) << "Profile " << index
            << " does not contain the same Password forms as Verifier Profile.";
    VLOG(1) << mismatch_details_stream.str();
  }
  return is_matching;
}

bool ProfilesContainSamePasswordForms(int index_a, int index_b) {
  std::vector<std::unique_ptr<PasswordForm>> forms_a =
      GetLogins(GetPasswordStore(index_a));
  std::vector<std::unique_ptr<PasswordForm>> forms_b =
      GetLogins(GetPasswordStore(index_b));
  ClearSyncDateField(&forms_a);
  ClearSyncDateField(&forms_b);

  std::ostringstream mismatch_details_stream;
  bool is_matching = password_manager::ContainsEqualPasswordFormsUnordered(
      forms_a, forms_b, &mismatch_details_stream);
  if (!is_matching) {
    VLOG(1) << "Password forms in Profile " << index_a
            << " (listed as 'expected forms' below)"
            << " do not match those in Profile " << index_b
            << " (listed as 'actual forms' below)";
    VLOG(1) << mismatch_details_stream.str();
  }
  return is_matching;
}

bool AllProfilesContainSamePasswordFormsAsVerifier() {
  for (int i = 0; i < test()->num_clients(); ++i) {
    if (!ProfileContainsSamePasswordFormsAsVerifier(i)) {
      DVLOG(1) << "Profile " << i << " does not contain the same password"
                                     " forms as the verifier.";
      return false;
    }
  }
  return true;
}

bool AllProfilesContainSamePasswordForms() {
  for (int i = 1; i < test()->num_clients(); ++i) {
    if (!ProfilesContainSamePasswordForms(0, i)) {
      DVLOG(1) << "Profile " << i << " does not contain the same password"
                                     " forms as Profile 0.";
      return false;
    }
  }
  return true;
}

int GetPasswordCount(int index) {
  return GetLogins(GetPasswordStore(index)).size();
}

int GetVerifierPasswordCount() {
  return GetLogins(GetVerifierPasswordStore()).size();
}

PasswordForm CreateTestPasswordForm(int index) {
  PasswordForm form;
  form.signon_realm = kFakeSignonRealm;
  form.origin = GURL(base::StringPrintf(kIndexedFakeOrigin, index));
  form.username_value =
      base::ASCIIToUTF16(base::StringPrintf("username%d", index));
  form.password_value =
      base::ASCIIToUTF16(base::StringPrintf("password%d", index));
  form.date_created = base::Time::Now();
  return form;
}

}  // namespace passwords_helper

SamePasswordFormsChecker::SamePasswordFormsChecker()
    : MultiClientStatusChangeChecker(
        sync_datatype_helper::test()->GetSyncServices()),
      in_progress_(false),
      needs_recheck_(false) {}

// This method needs protection against re-entrancy.
//
// This function indirectly calls GetLogins(), which starts a RunLoop on the UI
// thread.  This can be a problem, since the next task to execute could very
// well contain a ProfileSyncService::OnStateChanged() event, which would
// trigger another call to this here function, and start another layer of
// nested RunLoops.  That makes the StatusChangeChecker's Quit() method
// ineffective.
//
// The work-around is to not allow re-entrancy.  But we can't just drop
// IsExitConditionSatisifed() calls if one is already in progress.  Instead, we
// set a flag to ask the current execution of IsExitConditionSatisfied() to be
// re-run.  This ensures that the return value is always based on the most
// up-to-date state.
bool SamePasswordFormsChecker::IsExitConditionSatisfied() {
  if (in_progress_) {
    LOG(WARNING) << "Setting flag and returning early to prevent nesting.";
    needs_recheck_ = true;
    return false;
  }

  // Keep retrying until we get a good reading.
  bool result = false;
  in_progress_ = true;
  do {
    needs_recheck_ = false;
    result = passwords_helper::AllProfilesContainSamePasswordForms();
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

std::string SamePasswordFormsChecker::GetDebugMessage() const {
  return "Waiting for matching passwords";
}

SamePasswordFormsAsVerifierChecker::SamePasswordFormsAsVerifierChecker(int i)
    : SingleClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncService(i)),
      index_(i),
      in_progress_(false),
      needs_recheck_(false) {
}

// This method uses the same re-entrancy prevention trick as
// the SamePasswordFormsChecker.
bool SamePasswordFormsAsVerifierChecker::IsExitConditionSatisfied() {
  if (in_progress_) {
    LOG(WARNING) << "Setting flag and returning early to prevent nesting.";
    needs_recheck_ = true;
    return false;
  }

  // Keep retrying until we get a good reading.
  bool result = false;
  in_progress_ = true;
  do {
    needs_recheck_ = false;
    result =
        passwords_helper::ProfileContainsSamePasswordFormsAsVerifier(index_);
  } while (needs_recheck_);
  in_progress_ = false;
  return result;
}

std::string SamePasswordFormsAsVerifierChecker::GetDebugMessage() const {
  return "Waiting for passwords to match verifier";
}
