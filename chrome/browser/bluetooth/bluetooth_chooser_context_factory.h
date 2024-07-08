// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace permissions {
class BluetoothChooserContext;
}

class BluetoothChooserContextFactory : public ProfileKeyedServiceFactory {
 public:
  static permissions::BluetoothChooserContext* GetForProfile(Profile* profile);
  static permissions::BluetoothChooserContext* GetForProfileIfExists(
      Profile* profile);
  static BluetoothChooserContextFactory* GetInstance();

  // Move-only class.
  BluetoothChooserContextFactory(const BluetoothChooserContextFactory&) =
      delete;
  BluetoothChooserContextFactory& operator=(
      const BluetoothChooserContextFactory&) = delete;

 private:
  friend base::NoDestructor<BluetoothChooserContextFactory>;

  BluetoothChooserContextFactory();
  ~BluetoothChooserContextFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_BLUETOOTH_BLUETOOTH_CHOOSER_CONTEXT_FACTORY_H_
