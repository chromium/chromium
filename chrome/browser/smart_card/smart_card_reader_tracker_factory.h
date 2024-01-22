// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_FACTORY_H_
#define CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class SmartCardReaderTracker;

class SmartCardReaderTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static SmartCardReaderTracker& GetForProfile(Profile& profile);
  static SmartCardReaderTrackerFactory* GetInstance();

  SmartCardReaderTrackerFactory(const SmartCardReaderTrackerFactory&) = delete;
  SmartCardReaderTrackerFactory& operator=(
      const SmartCardReaderTrackerFactory&) = delete;

 private:
  friend base::NoDestructor<SmartCardReaderTrackerFactory>;

  SmartCardReaderTrackerFactory();
  ~SmartCardReaderTrackerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SMART_CARD_SMART_CARD_READER_TRACKER_FACTORY_H_
