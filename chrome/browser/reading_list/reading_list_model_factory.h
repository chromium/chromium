// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_
#define CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class ReadingListModel;

// Singleton that owns all ReadingListModels and associates them with
// BrowserContexts.
class ReadingListModelFactory : public ProfileKeyedServiceFactory {
 public:
  ReadingListModelFactory(const ReadingListModelFactory&) = delete;
  ReadingListModelFactory& operator=(const ReadingListModelFactory&) = delete;

  static ReadingListModel* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static ReadingListModelFactory* GetInstance();

  static BrowserContextKeyedServiceFactory::TestingFactory
  GetDefaultFactoryForTesting();

 private:
  friend base::NoDestructor<ReadingListModelFactory>;

  ReadingListModelFactory();
  ~ReadingListModelFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_
