// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include "base/no_destructor.h"

class Profile;

namespace syncer {
class SyncInvalidationsService;
}  // namespace syncer

class SyncInvalidationsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  SyncInvalidationsServiceFactory(const SyncInvalidationsServiceFactory&) =
      delete;
  SyncInvalidationsServiceFactory& operator=(
      const SyncInvalidationsServiceFactory&) = delete;

  // Returned value may be nullptr in case if sync invalidations are disabled or
  // not supported.
  static syncer::SyncInvalidationsService* GetForProfile(Profile* profile);

  static SyncInvalidationsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<SyncInvalidationsServiceFactory>;

  SyncInvalidationsServiceFactory();
  ~SyncInvalidationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SYNC_SYNC_INVALIDATIONS_SERVICE_FACTORY_H_
