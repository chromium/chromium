// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class IntentPickerAutoDisplayService;
class Profile;

class IntentPickerAutoDisplayServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static IntentPickerAutoDisplayService* GetForProfile(Profile* profile);
  static IntentPickerAutoDisplayServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      IntentPickerAutoDisplayServiceFactory>;

  IntentPickerAutoDisplayServiceFactory();
  ~IntentPickerAutoDisplayServiceFactory() override;

  // BrowserContextKeyedServiceFactory Overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerAutoDisplayServiceFactory);
};

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_INTENT_PICKER_AUTO_DISPLAY_SERVICE_FACTORY_H_
