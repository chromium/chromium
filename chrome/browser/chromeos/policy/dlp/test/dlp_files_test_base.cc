// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"

namespace policy {

DlpFilesTestBase::DlpFilesTestBase()
    : task_environment_(std::make_unique<content::BrowserTaskEnvironment>()) {}
DlpFilesTestBase::DlpFilesTestBase(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
    : task_environment_(std::move(task_environment)) {}
DlpFilesTestBase::~DlpFilesTestBase() = default;

void DlpFilesTestBase::SetUp() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
  scoped_profile_ = std::make_unique<TestingProfile>();
  profile_ = scoped_profile_.get();
  AccountId account_id =
      AccountId::FromUserEmailGaiaId("test@example.com", "12345");
  profile_->SetIsNewProfile(true);
  user_manager::User* user =
      user_manager->AddUserWithAffiliationAndTypeAndProfile(
          account_id,
          /*is_affiliated=*/false, user_manager::UserType::kRegular, profile_);
  user_manager->UserLoggedIn(account_id, user->username_hash(),
                             /*browser_restart=*/false,
                             /*is_child=*/false);
  user_manager->SimulateUserProfileLoad(account_id);
  scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
      std::move(user_manager));
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  ASSERT_TRUE(profile_manager_.SetUp());
  profile_ = profile_manager_.CreateTestingProfile("user", true);
#endif
  policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
      profile_, base::BindRepeating(&DlpFilesTestBase::SetDlpRulesManager,
                                    base::Unretained(this)));
  ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
  ASSERT_TRUE(rules_manager_);
}

void DlpFilesTestBase::TearDown() {
  profile_ = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_user_manager_.reset();
  scoped_profile_.reset();
#else  // BUILDFLAG(IS_CHROMEOS_LACROS)
  profile_manager_.DeleteAllTestingProfiles();
#endif
}

std::unique_ptr<KeyedService> DlpFilesTestBase::SetDlpRulesManager(
    content::BrowserContext* context) {
  auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>(
      Profile::FromBrowserContext(context));
  rules_manager_ = dlp_rules_manager.get();
  return dlp_rules_manager;
}

}  // namespace policy
