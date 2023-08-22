// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PushMessagingServiceImpl;

class PushMessagingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static PushMessagingServiceImpl* GetForProfile(
      content::BrowserContext* profile);
  static PushMessagingServiceFactory* GetInstance();

  PushMessagingServiceFactory(const PushMessagingServiceFactory&) = delete;
  PushMessagingServiceFactory& operator=(const PushMessagingServiceFactory&) =
      delete;

  // Undo a previous call to SetTestingFactory, such that subsequent calls to
  // GetForProfile get a real push service.
  void RestoreFactoryForTests(content::BrowserContext* context);

 private:
  friend base::NoDestructor<PushMessagingServiceFactory>;

  PushMessagingServiceFactory();
  ~PushMessagingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_SERVICE_FACTORY_H_
