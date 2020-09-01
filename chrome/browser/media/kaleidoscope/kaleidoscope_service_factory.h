// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace kaleidoscope {

class KaleidoscopeService;

class KaleidoscopeServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static KaleidoscopeService* GetForProfile(Profile* profile);
  static KaleidoscopeServiceFactory* GetInstance();

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend class base::NoDestructor<KaleidoscopeServiceFactory>;

  KaleidoscopeServiceFactory();
  ~KaleidoscopeServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace kaleidoscope

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_SERVICE_FACTORY_H_
