// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

using password_manager::PasswordForm;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetAccountPasswordStoreInterface;
using passwords_helper::GetPasswordCount;
using sync_timing_helper::TimeUntilQuiescence;

static const int kNumPasswords = 150;

namespace {

constexpr char kMetricPrefixPasswords[] = "Passwords.";
constexpr char kMetricAddPasswordsSyncTime[] = "add_passwords_sync_time";
constexpr char kMetricUpdatePasswordsSyncTime[] = "update_passwords_sync_time";
constexpr char kMetricDeletePasswordsSyncTime[] = "delete_passwords_sync_time";

perf_test::PerfResultReporter SetUpReporter(const std::string& story) {
  perf_test::PerfResultReporter reporter(kMetricPrefixPasswords, story);
  reporter.RegisterImportantMetric(kMetricAddPasswordsSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricUpdatePasswordsSyncTime, "ms");
  reporter.RegisterImportantMetric(kMetricDeletePasswordsSyncTime, "ms");
  return reporter;
}

}  // namespace

class PasswordsSyncPerfTest : public SyncTest {
 public:
  PasswordsSyncPerfTest() : SyncTest(TWO_CLIENT) {}

  PasswordsSyncPerfTest(const PasswordsSyncPerfTest&) = delete;
  PasswordsSyncPerfTest& operator=(const PasswordsSyncPerfTest&) = delete;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return SetupSyncMode::kSyncTransportOnly;
  }

  // Adds |num_logins| new unique passwords to |profile|.
  void AddLogins(int profile, int num_logins);

  // Updates the password for all logins for |profile|.
  void UpdateLogins(int profile);

  // Removes all logins for |profile|.
  void RemoveLogins(int profile);

 private:
  // Returns a new unique login.
  PasswordForm NextLogin();

  // Returns a new unique password value.
  std::string NextPassword();

  int password_number_ = 0;
};

void PasswordsSyncPerfTest::AddLogins(int profile, int num_logins) {
  for (int i = 0; i < num_logins; ++i) {
    GetAccountPasswordStoreInterface(profile)->AddLogin(NextLogin());
  }
  // Don't proceed before all additions happen on the background thread.
  // Call GetPasswordCount() because it blocks on the background thread.
  GetPasswordCount(profile, PasswordForm::Store::kAccountStore);
}

void PasswordsSyncPerfTest::UpdateLogins(int profile) {
  std::vector<std::unique_ptr<PasswordForm>> logins =
      passwords_helper::GetLogins(GetAccountPasswordStoreInterface(profile));
  for (std::unique_ptr<PasswordForm>& login : logins) {
    login->password_value = base::ASCIIToUTF16(NextPassword());
    GetAccountPasswordStoreInterface(profile)->UpdateLogin(*login);
  }
  // Don't proceed before all updates happen on the background thread.
  // Call GetPasswordCount() because it blocks on the background thread.
  GetPasswordCount(profile, PasswordForm::Store::kAccountStore);
}

void PasswordsSyncPerfTest::RemoveLogins(int profile) {
  passwords_helper::RemoveLogins(GetAccountPasswordStoreInterface(profile));
  // Don't proceed before all removals happen on the background thread.
  // Call GetPasswordCount() because it blocks on the background thread.
  GetPasswordCount(profile, PasswordForm::Store::kAccountStore);
}

PasswordForm PasswordsSyncPerfTest::NextLogin() {
  return CreateTestPasswordForm(password_number_++,
                                PasswordForm::Store::kAccountStore);
}

std::string PasswordsSyncPerfTest::NextPassword() {
  return base::StringPrintf("password%d", password_number_++);
}

IN_PROC_BROWSER_TEST_F(PasswordsSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync());

  perf_test::PerfResultReporter reporter =
      SetUpReporter(base::NumberToString(kNumPasswords) + "_passwords");
  AddLogins(0, kNumPasswords);
  base::TimeDelta dt = TimeUntilQuiescence(GetSyncClients());
  ASSERT_EQ(kNumPasswords,
            GetPasswordCount(1, PasswordForm::Store::kAccountStore));
  reporter.AddResult(kMetricAddPasswordsSyncTime, dt);

  UpdateLogins(0);
  dt = TimeUntilQuiescence(GetSyncClients());
  ASSERT_EQ(kNumPasswords,
            GetPasswordCount(1, PasswordForm::Store::kAccountStore));
  reporter.AddResult(kMetricUpdatePasswordsSyncTime, dt);

  RemoveLogins(0);
  dt = TimeUntilQuiescence(GetSyncClients());
  ASSERT_EQ(0, GetPasswordCount(1, PasswordForm::Store::kAccountStore));
  reporter.AddResult(kMetricDeletePasswordsSyncTime, dt);
}
