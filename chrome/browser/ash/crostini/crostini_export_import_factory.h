// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace crostini {

class CrostiniExportImport;

// Singleton factory to create/retrieve the CrostiniExportImport
// per Profile.
class CrostiniExportImportFactory : public ProfileKeyedServiceFactory {
 public:
  static CrostiniExportImport* GetForProfile(Profile* profile);

  static CrostiniExportImportFactory* GetInstance();

  CrostiniExportImportFactory(const CrostiniExportImportFactory&) = delete;
  CrostiniExportImportFactory& operator=(const CrostiniExportImportFactory&) =
      delete;

 private:
  friend class base::NoDestructor<CrostiniExportImportFactory>;

  CrostiniExportImportFactory();
  ~CrostiniExportImportFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_FACTORY_H_
