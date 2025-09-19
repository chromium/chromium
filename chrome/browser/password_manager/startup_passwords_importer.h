// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORTER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class Profile;

// This tool takes csv file as an input and imports all the credentials present
// in the csv file. Currently we only import to the profile store.
class StartupPasswordsImporter {
 public:
  explicit StartupPasswordsImporter(Profile* profile);
  ~StartupPasswordsImporter();

  // Not copyable or movable.
  StartupPasswordsImporter(const StartupPasswordsImporter&) = delete;
  StartupPasswordsImporter& operator=(const StartupPasswordsImporter&) = delete;

  // Starts the asynchronous import process.
  void StartImport();

 private:
  // Callback for when the import process finishes.
  void OnImportFinished(const password_manager::ImportResults& results);

  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_;
  std::unique_ptr<password_manager::PasswordImporter> password_importer_;

  base::WeakPtrFactory<StartupPasswordsImporter> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORTER_H_
