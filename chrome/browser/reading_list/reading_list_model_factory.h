// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_
#define CHROME_BROWSER_READING_LIST_READING_LIST_MODEL_FACTORY_H_

#include "build/build_config.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/reading_list/core/dual_reading_list_model.h"
#include "components/reading_list/core/reading_list_model.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
class NoDestructor;
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
