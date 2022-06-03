// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class Profile;

namespace federated_learning {

class FlocIdProvider;

// Singleton that owns all FlocIdProvider and associates them with Profiles.
class FlocIdProviderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static FlocIdProvider* GetForProfile(Profile* profile);
  static FlocIdProviderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<FlocIdProviderFactory>;

  FlocIdProviderFactory();
  ~FlocIdProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace federated_learning

#endif  // CHROME_BROWSER_FEDERATED_LEARNING_FLOC_ID_PROVIDER_FACTORY_H_
