// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_FILES_TEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_FILES_TEST_BASE_H_

#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_rules_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> scoped_profile_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
#endif
  raw_ptr<TestingProfile> profile_;

  raw_ptr<MockDlpRulesManager, DanglingUntriaged> rules_manager_ = nullptr;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_TEST_DLP_FILES_TEST_BASE_H_
