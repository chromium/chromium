// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_FILES_TEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_FILES_TEST_BASE_H_

#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace policy {

// Base class for DLP files testing. Sets `profile_` on both Ash and Lacros and
// sets `rules_manager_` as DlpRulesManager instance for `profile_`.
class DlpFilesTestBase : public testing::Test {
 public:
  DlpFilesTestBase(const DlpFilesTestBase&) = delete;
  DlpFilesTestBase& operator=(const DlpFilesTestBase&) = delete;

 protected:
  DlpFilesTestBase();
  DlpFilesTestBase(
      std::unique_ptr<content::BrowserTaskEnvironment> task_environment);
  ~DlpFilesTestBase() override;

  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context);

  Profile* profile() { return profile_.get(); }
  MockDlpRulesManager* rules_manager() { return rules_manager_; }

 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
  std::unique_ptr<Profile> profile_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_;
  raw_ptr<MockDlpRulesManager, DanglingUntriaged> rules_manager_ = nullptr;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_FILES_TEST_BASE_H_
