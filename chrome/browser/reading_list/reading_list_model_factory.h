// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_
#define CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ReadingListModel;

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

namespace reading_list {
class DualReadingListModel;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Singleton that owns all ReadingListModels and associates them with
// BrowserContexts.
class ReadingListModelFactory : public ProfileKeyedServiceFactory {
 public:
  ReadingListModelFactory(const ReadingListModelFactory&) = delete;
  ReadingListModelFactory& operator=(const ReadingListModelFactory&) = delete;

  static ReadingListModel* GetForBrowserContext(
      content::BrowserContext* context);

  static reading_list::DualReadingListModel*
  GetAsDualReadingListForBrowserContext(content::BrowserContext* context);

  // Returns whether a ReadingListModel was created for `profile`.
  // GetForBrowserContext() can't be used because it creates the model if one
  // doesn't exist yet.
  static bool HasModel(content::BrowserContext* context);

  static ReadingListModelFactory* GetInstance();

  static BrowserContextKeyedServiceFactory::TestingFactory
  GetDefaultFactoryForTesting();

 private:
  friend base::NoDestructor<ReadingListModelFactory>;

  ReadingListModelFactory();
  ~ReadingListModelFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_
