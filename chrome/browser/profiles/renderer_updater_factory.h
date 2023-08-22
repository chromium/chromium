// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_RENDERER_UPDATER_FACTORY_H_
#define CHROME_BROWSER_PROFILES_RENDERER_UPDATER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class RendererUpdater;

// Singleton that creates/deletes RendererUpdater as new Profiles are
// created/shutdown.
class RendererUpdaterFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the RendererUpdaterFactory singleton.
  static RendererUpdaterFactory* GetInstance();

  // Returns the instance of RendererUpdater for the passed |profile|.
  static RendererUpdater* GetForProfile(Profile* profile);

  RendererUpdaterFactory(const RendererUpdaterFactory&) = delete;
  RendererUpdaterFactory& operator=(const RendererUpdaterFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<RendererUpdaterFactory>;

  RendererUpdaterFactory();
  ~RendererUpdaterFactory() override;
};

#endif  // CHROME_BROWSER_PROFILES_RENDERER_UPDATER_FACTORY_H_
