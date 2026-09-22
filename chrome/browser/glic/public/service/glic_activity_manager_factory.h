// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/glic/public/service/glic_activity_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace glic {

// Handles the glic activity manager in chrome, only regular, non-OTR profiles
// are supported.
class GlicActivityManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static GlicActivityManagerFactory* GetInstance();
  static GlicActivityManager* GetForProfile(Profile* profile);

  GlicActivityManagerFactory(const GlicActivityManagerFactory&) = delete;
  GlicActivityManagerFactory& operator=(const GlicActivityManagerFactory&) =
      delete;

 private:
  friend base::NoDestructor<GlicActivityManagerFactory>;
  GlicActivityManagerFactory();
  ~GlicActivityManagerFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_SERVICE_GLIC_ACTIVITY_MANAGER_FACTORY_H_
