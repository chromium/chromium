// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_FAKE_SMART_CARD_DEVICE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SMART_CARD_FAKE_SMART_CARD_DEVICE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FakeSmartCardDeviceService;

class FakeSmartCardDeviceServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FakeSmartCardDeviceService& GetForProfile(Profile& profile);
  static FakeSmartCardDeviceServiceFactory& GetInstance();

 private:
  friend base::NoDestructor<FakeSmartCardDeviceServiceFactory>;

  FakeSmartCardDeviceServiceFactory();
  ~FakeSmartCardDeviceServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SMART_CARD_FAKE_SMART_CARD_DEVICE_SERVICE_FACTORY_H_
