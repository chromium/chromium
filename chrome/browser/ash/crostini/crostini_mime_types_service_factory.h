// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MIME_TYPES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MIME_TYPES_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace crostini {

class CrostiniMimeTypesService;

class CrostiniMimeTypesServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniMimeTypesService* GetForProfile(Profile* profile);
  static CrostiniMimeTypesServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<CrostiniMimeTypesServiceFactory>;

  CrostiniMimeTypesServiceFactory();
  ~CrostiniMimeTypesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CrostiniMimeTypesServiceFactory);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MIME_TYPES_SERVICE_FACTORY_H_
