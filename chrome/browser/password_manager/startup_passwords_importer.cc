// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/startup_passwords_importer.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/import/import_results.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

StartupPasswordsImporter::StartupPasswordsImporter(Profile* profile)
    : profile_(profile) {
  affiliations::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile_);

  scoped_refptr<password_manager::PasswordStoreInterface> profile_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);

  scoped_refptr<password_manager::PasswordStoreInterface> account_store =
      AccountPasswordStoreFactory::GetForProfile(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);

  saved_passwords_presenter_ =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          affiliation_service, profile_store, account_store);

  password_importer_ = std::make_unique<password_manager::PasswordImporter>(
      saved_passwords_presenter_.get());
}

StartupPasswordsImporter::~StartupPasswordsImporter() = default;

void StartupPasswordsImporter::StartImport() {
  VLOG(1) << "StartupPasswordsImporter::StartImport called.";

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  CHECK(command_line->HasSwitch(switches::kImportPasswords));

  base::FilePath csv_file_path =
      command_line->GetSwitchValuePath(switches::kImportPasswords);

  if (csv_file_path.empty()) {
    LOG(ERROR) << "Empty file path provided for " << switches::kImportPasswords;
    return;
  }

  password_importer_->Import(
      csv_file_path, password_manager::PasswordForm::Store::kProfileStore,
      base::BindOnce(&StartupPasswordsImporter::OnImportFinished,
                     base::Unretained(this)));
}

void StartupPasswordsImporter::OnImportFinished(
    const password_manager::ImportResults& results) {
  VLOG(1) << "Password import finished with status: "
          << static_cast<int>(results.status);

  if (results.status == password_manager::ImportResults::Status::SUCCESS) {
    VLOG(1) << "Passwords import successful.";
  } else {
    LOG(ERROR) << "Passwords import failed with status code: "
               << static_cast<int>(results.status);
  }
}
