// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORT_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORT_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

class Profile;
class StartupPasswordsImporter;

class StartupPasswordsImportService : public KeyedService {
 public:
  explicit StartupPasswordsImportService(Profile* profile);
  ~StartupPasswordsImportService() override;

  // Not copyable or movable.
  StartupPasswordsImportService(const StartupPasswordsImportService&) = delete;
  StartupPasswordsImportService& operator=(
      const StartupPasswordsImportService&) = delete;

 private:
  // KeyedService implementation.
  void Shutdown() override;

  std::unique_ptr<StartupPasswordsImporter> importer_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORT_SERVICE_H_
