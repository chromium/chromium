// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class HidChooserContext;
class Profile;

class HidChooserContextFactory : public ProfileKeyedServiceFactory {
 public:
  static HidChooserContext* GetForProfile(Profile* profile);
  static HidChooserContext* GetForProfileIfExists(Profile* profile);
  static HidChooserContextFactory* GetInstance();

  HidChooserContextFactory(const HidChooserContextFactory&) = delete;
  HidChooserContextFactory& operator=(const HidChooserContextFactory&) = delete;

 private:
  friend base::NoDestructor<HidChooserContextFactory>;

  HidChooserContextFactory();
  ~HidChooserContextFactory() override;

  // BrowserContextKeyedBaseFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_
