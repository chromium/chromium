// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_FACTORY_H_
#define CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

class HidPolicyAllowedDevices;

class HidPolicyAllowedDevicesFactory : public ProfileKeyedServiceFactory {
 public:
  static HidPolicyAllowedDevices* GetForProfile(Profile* profile);
  static HidPolicyAllowedDevicesFactory* GetInstance();

  HidPolicyAllowedDevicesFactory(const HidPolicyAllowedDevicesFactory&) =
      delete;
  HidPolicyAllowedDevicesFactory& operator=(
      const HidPolicyAllowedDevicesFactory&) = delete;

 private:
  friend class base::NoDestructor<HidPolicyAllowedDevicesFactory>;

  HidPolicyAllowedDevicesFactory();
  ~HidPolicyAllowedDevicesFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_FACTORY_H_
