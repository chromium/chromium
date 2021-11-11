// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class HidChooserContext;
class Profile;

class HidChooserContextFactory : public BrowserContextKeyedServiceFactory {
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
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
};

#endif  // CHROME_BROWSER_HID_HID_CHOOSER_CONTEXT_FACTORY_H_
