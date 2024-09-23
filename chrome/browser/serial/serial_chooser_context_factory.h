// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SerialChooserContext;
class Profile;

class SerialChooserContextFactory : public ProfileKeyedServiceFactory {
 public:
  static SerialChooserContext* GetForProfile(Profile* profile);
  static SerialChooserContext* GetForProfileIfExists(Profile* profile);
  static SerialChooserContextFactory* GetInstance();

  SerialChooserContextFactory(const SerialChooserContextFactory&) = delete;
  SerialChooserContextFactory& operator=(const SerialChooserContextFactory&) =
      delete;

 private:
  friend base::NoDestructor<SerialChooserContextFactory>;

  SerialChooserContextFactory();
  ~SerialChooserContextFactory() override;

  // BrowserContextKeyedServiceFactory methods:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SERIAL_SERIAL_CHOOSER_CONTEXT_FACTORY_H_
