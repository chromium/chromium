// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ContentIndexProviderImpl;
class Profile;

class ContentIndexProviderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ContentIndexProviderImpl* GetForProfile(Profile* profile);
  static ContentIndexProviderFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ContentIndexProviderFactory>;

  ContentIndexProviderFactory();
  ~ContentIndexProviderFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ContentIndexProviderFactory);
};

#endif  // CHROME_BROWSER_CONTENT_INDEX_CONTENT_INDEX_PROVIDER_FACTORY_H_
