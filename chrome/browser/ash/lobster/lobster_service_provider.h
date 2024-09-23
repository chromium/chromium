// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_PROVIDER_H_
#define CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_PROVIDER_H_

#include "base/no_destructor.h"
#include "chrome/browser/ash/lobster/lobster_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

class LobsterServiceProvider : public ProfileKeyedServiceFactory {
 public:
  static LobsterService* GetForProfile(Profile* profile);
  static LobsterServiceProvider* GetInstance();
  static std::unique_ptr<KeyedService> BuildInstanceFor(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<LobsterServiceProvider>;

  LobsterServiceProvider();
  ~LobsterServiceProvider() override;

  // BrowserContextKeyedServiceFactory overrides
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_LOBSTER_SERVICE_PROVIDER_H_
