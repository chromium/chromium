// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_FEATURES_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_FEATURES_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/borealis/borealis_features.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace borealis {

// Implementation of the factory used to access profile-keyed instances of the
// features service.
class BorealisFeaturesFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BorealisFeatures* GetForProfile(Profile* profile);

  static BorealisFeaturesFactory* GetInstance();

  // Can not be moved or copied.
  BorealisFeaturesFactory(const BorealisFeaturesFactory&) = delete;
  BorealisFeaturesFactory(BorealisFeaturesFactory&&) = delete;
  BorealisFeaturesFactory& operator=(const BorealisFeaturesFactory&) = delete;
  BorealisFeaturesFactory& operator=(BorealisFeaturesFactory&&) = delete;

 private:
  friend base::NoDestructor<BorealisFeaturesFactory>;

  BorealisFeaturesFactory();
  ~BorealisFeaturesFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_CHROMEOS_BOREALIS_BOREALIS_FEATURES_FACTORY_H_
