// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DiceResponseHandler;

class DiceResponseHandlerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the DiceResponseHandler associated with this profile.
  // May return nullptr if there is none (e.g. in incognito).
  static DiceResponseHandler* GetForProfile(Profile* profile);

  // Returns the factory singleton instance.
  static DiceResponseHandlerFactory* GetInstance();

  DiceResponseHandlerFactory(const DiceResponseHandlerFactory&) = delete;
  DiceResponseHandlerFactory& operator=(const DiceResponseHandlerFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<DiceResponseHandlerFactory>;

  DiceResponseHandlerFactory();
  ~DiceResponseHandlerFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_RESPONSE_HANDLER_FACTORY_H_
