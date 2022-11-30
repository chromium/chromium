// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ReadingListManager;

// A factory to create the ReadingListManager singleton.
class ReadingListManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static ReadingListManagerFactory* GetInstance();
  static ReadingListManager* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<ReadingListManagerFactory>;

  ReadingListManagerFactory();
  ~ReadingListManagerFactory() override;

  ReadingListManagerFactory(const ReadingListManagerFactory&) = delete;
  ReadingListManagerFactory& operator=(const ReadingListManagerFactory&) =
      delete;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ANDROID_READING_LIST_READING_LIST_MANAGER_FACTORY_H_
