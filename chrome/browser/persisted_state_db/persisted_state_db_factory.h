// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_FACTORY_H_
#define CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class PersistedStateDB;

// Factory to create on PersistedStateDB per profile. Incognito is currently
// not supported and the factory will return nullptr for an incognito profile.
class PersistedStateDBFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Acquire instance of PersistedStateDBFactory
  static PersistedStateDBFactory* GetInstance();

  // Acquire PersistedStateDB - there is one per profile.
  static PersistedStateDB* GetForProfile(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<PersistedStateDBFactory>;

  PersistedStateDBFactory();
  ~PersistedStateDBFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERSISTED_STATE_DB_PERSISTED_STATE_DB_FACTORY_H_
