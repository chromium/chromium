// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/startup_passwords_import_service.h"

#include "base/logging.h"
#include "chrome/browser/password_manager/startup_passwords_importer.h"
#include "chrome/browser/profiles/profile.h"

StartupPasswordsImportService::StartupPasswordsImportService(Profile* profile) {
  VLOG(1) << "StartupPasswordsImportService created, instantiating and "
             "starting importer.";
  importer_ = std::make_unique<StartupPasswordsImporter>(profile);
  importer_->StartImport();
}

StartupPasswordsImportService::~StartupPasswordsImportService() = default;

void StartupPasswordsImportService::Shutdown() {
  // Opportunity to cancel any ongoing background tasks if necessary.
  // The importer_ will be destroyed automatically.
  VLOG(1) << "StartupPasswordsImportService shutting down.";
}
