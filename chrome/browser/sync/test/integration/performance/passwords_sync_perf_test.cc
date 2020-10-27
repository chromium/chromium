// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/performance/sync_timing_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/password_manager/core/browser/password_store.h"
#include "content/public/test/browser_test.h"
#include "testing/perf/perf_result_reporter.h"

using passwords_helper::AddLogin;
using passwords_helper::CreateTestPasswordForm;
using passwords_helper::GetPasswordCount;
using passwords_helper::GetPasswordStore;
using passwords_helper::UpdateLogin;
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
  PasswordsSyncPerfTest() : SyncTest(TWO_CLIENT), password_number_(0) {}

  // Adds |num_logins| new unique passwords to |profile|.
  void AddLogins(int profile, int num_logins);

  // Updates the password for all logins for |profile|.
  void UpdateLogins(int profile);

  // Removes all logins for |profile|.
  void RemoveLogins(int profile);

 private:
  // Returns a new unique login.
  password_manager::PasswordForm NextLogin();

  // Returns a new unique password value.
  std::string NextPassword();

  int password_number_;
  DISALLOW_COPY_AND_ASSIGN(PasswordsSyncPerfTest);
};

void PasswordsSyncPerfTest::AddLogins(int profile, int num_logins) {
  for (int i = 0; i < num_logins; ++i) {
    AddLogin(GetPasswordStore(profile), NextLogin());
  }
}

void PasswordsSyncPerfTest::UpdateLogins(int profile) {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> logins =
      passwords_helper::GetLogins(GetPasswordStore(profile));
  for (auto& login : logins) {
    login->password_value = base::ASCIIToUTF16(NextPassword());
    UpdateLogin(GetPasswordStore(profile), *login);
  }
}

void PasswordsSyncPerfTest::RemoveLogins(int profile) {
  passwords_helper::RemoveLogins(GetPasswordStore(profile));
}

password_manager::PasswordForm PasswordsSyncPerfTest::NextLogin() {
  return CreateTestPasswordForm(password_number_++);
}

std::string PasswordsSyncPerfTest::NextPassword() {
  return base::StringPrintf("password%d", password_number_++);
}

IN_PROC_BROWSER_TEST_F(PasswordsSyncPerfTest, P0) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  auto reporter =
      SetUpReporter(base::NumberToString(kNumPasswords) + "_passwords");
  AddLogins(0, kNumPasswords);
  base::TimeDelta dt = TimeUntilQuiescence(GetSyncClients());
  ASSERT_EQ(kNumPasswords, GetPasswordCount(1));
  reporter.AddResult(kMetricAddPasswordsSyncTime, dt);

  UpdateLogins(0);
  dt = TimeUntilQuiescence(GetSyncClients());
  ASSERT_EQ(kNumPasswords, GetPasswordCount(1));
  reporter.AddResult(kMetricUpdatePasswordsSyncTime, dt);

  RemoveLogins(0);
  dt = TimeUntilQuiescence(GetSyncClients());
  ASSERT_EQ(0, GetPasswordCount(1));
  reporter.AddResult(kMetricDeletePasswordsSyncTime, dt);
}
