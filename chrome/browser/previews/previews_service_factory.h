// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class PreviewsService;
class Profile;

// LazyInstance that owns all PreviewsServices and associates them with
// Profiles.
class PreviewsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the PreviewsService instance for |profile|.
  static PreviewsService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all PreviewsServices.
  static PreviewsServiceFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<PreviewsServiceFactory>;

  PreviewsServiceFactory();
  ~PreviewsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PreviewsServiceFactory);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_FACTORY_H_
