// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"

#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "google_apis/gaia/gaia_id.h"

namespace policy {

namespace {

constexpr AccountId::Literal kAccountId =
    AccountId::Literal::FromUserEmailGaiaId("test@example.com",
                                            GaiaId::Literal("12345"));

}  // namespace

DlpFilesTestBase::DlpFilesTestBase()
    : task_environment_(std::make_unique<content::BrowserTaskEnvironment>()) {}
DlpFilesTestBase::DlpFilesTestBase(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {}
DlpFilesTestBase::~DlpFilesTestBase() = default;

void DlpFilesTestBase::SetUp() {
  auto* browser_process = TestingBrowserProcess::GetGlobal();
  user_session_test_environment_ =
      std::make_unique<ash::test::UserSessionTestEnvironment>(
          browser_process->local_state(),
          std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
              browser_process));
  ASSERT_TRUE(user_session_test_environment_->AddRegularUser(kAccountId));
  user_session_test_environment_->LogIn(kAccountId);

  auto* testing_profile =
      static_cast<TestingProfile*>(Profile::FromBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              kAccountId)));
  ASSERT_TRUE(testing_profile);
  testing_profile->SetIsNewProfile(true);
  profile_ = testing_profile;

  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      profile_, base::BindRepeating(&DlpFilesTestBase::SetDlpRulesManager,
                                    base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ASSERT_TRUE(rules_manager_);
}

void DlpFilesTestBase::TearDown() {
  rules_manager_ = nullptr;
  profile_ = nullptr;
  user_session_test_environment_.reset();
}

std::unique_ptr<KeyedService> DlpFilesTestBase::SetDlpRulesManager(
    content::BrowserContext* context) {
  auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>(
      Profile::FromBrowserContext(context));
  rules_manager_ = dlp_rules_manager.get();
  return dlp_rules_manager;
}

}  // namespace policy
