// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace borealis {

class BorealisService;

// Implementation of the factory used to access profile-keyed instances of the
// features service.
class BorealisServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BorealisService* GetForProfile(Profile* profile);

  static BorealisServiceFactory* GetInstance();

  // Can not be moved or copied.
  BorealisServiceFactory(const BorealisServiceFactory&) = delete;
  BorealisServiceFactory& operator=(const BorealisServiceFactory&) = delete;

 private:
  friend base::NoDestructor<BorealisServiceFactory>;

  BorealisServiceFactory();
  ~BorealisServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_FACTORY_H_
