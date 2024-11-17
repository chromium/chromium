// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/test/base/testing_profile.h"

namespace policy {

DlpFilesTestBase::DlpFilesTestBase()
    : task_environment_(std::make_unique<content::BrowserTaskEnvironment>()) {}
DlpFilesTestBase::DlpFilesTestBase(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {}
DlpFilesTestBase::~DlpFilesTestBase() = default;

void DlpFilesTestBase::SetUp() {
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("test@example.com", "12345");

  auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
  {
    auto testing_profile = std::make_unique<TestingProfile>();
    testing_profile->SetIsNewProfile(true);
    profile_ = std::move(testing_profile);
  }
  user_manager::User* user =
      user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id,
          /*is_affiliated=*/false, user_manager::UserType::kRegular,
          profile_.get());
  user_manager->UserLoggedIn(account_id, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  user_manager->SimulateUserProfileLoad(account_id);
  user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(user_manager));
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      profile_.get(), base::BindRepeating(&DlpFilesTestBase::SetDlpRulesManager,
                                          base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ASSERT_TRUE(rules_manager_);
}

void DlpFilesTestBase::TearDown() {
  user_manager_.reset();
  profile_.reset();
}

std::unique_ptr<KeyedService> DlpFilesTestBase::SetDlpRulesManager(
    content::BrowserContext* context) {
  auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>(
      Profile::FromBrowserContext(context));
  rules_manager_ = dlp_rules_manager.get();
  return dlp_rules_manager;
}

}  // namespace policy
