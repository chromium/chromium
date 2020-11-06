// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class RepeatableQueriesService;
class Profile;

class RepeatableQueriesServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the RepeatableQueriesService for |profile|.
  static RepeatableQueriesService* GetForProfile(Profile* profile);

  static RepeatableQueriesServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<RepeatableQueriesServiceFactory>;

  RepeatableQueriesServiceFactory();
  ~RepeatableQueriesServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(RepeatableQueriesServiceFactory);
};

#endif  // CHROME_BROWSER_SEARCH_REPEATABLE_QUERIES_REPEATABLE_QUERIES_SERVICE_FACTORY_H_
