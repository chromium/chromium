// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_CONTEXTUAL_SESSION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OMNIBOX_CONTEXTUAL_SESSION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/omnibox/composebox/contextual_session_service.h"

class Profile;

class ContextualSessionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ContextualSessionService* GetForProfile(Profile* profile);
  static ContextualSessionServiceFactory* GetInstance();

  ContextualSessionServiceFactory(const ContextualSessionServiceFactory&) =
      delete;
  ContextualSessionServiceFactory& operator=(
      const ContextualSessionServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<ContextualSessionServiceFactory>;

  ContextualSessionServiceFactory();
  ~ContextualSessionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_OMNIBOX_CONTEXTUAL_SESSION_SERVICE_FACTORY_H_
