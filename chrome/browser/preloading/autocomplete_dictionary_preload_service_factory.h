// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_AUTOCOMPLETE_DICTIONARY_PRELOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRELOADING_AUTOCOMPLETE_DICTIONARY_PRELOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class AutocompleteDictionaryPreloadService;
class Profile;

// Factory to create and manage AutocompleteDictionaryPreloadService.
class AutocompleteDictionaryPreloadServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static AutocompleteDictionaryPreloadService* GetForProfile(Profile* profile);

  static AutocompleteDictionaryPreloadServiceFactory* GetInstance();

  // Not movable nor copyable.
  AutocompleteDictionaryPreloadServiceFactory(
      const AutocompleteDictionaryPreloadServiceFactory&&) = delete;
  AutocompleteDictionaryPreloadServiceFactory& operator=(
      const AutocompleteDictionaryPreloadServiceFactory&&) = delete;
  AutocompleteDictionaryPreloadServiceFactory(
      const AutocompleteDictionaryPreloadServiceFactory&) = delete;
  AutocompleteDictionaryPreloadServiceFactory& operator=(
      const AutocompleteDictionaryPreloadServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AutocompleteDictionaryPreloadServiceFactory>;

  AutocompleteDictionaryPreloadServiceFactory();
  ~AutocompleteDictionaryPreloadServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PRELOADING_AUTOCOMPLETE_DICTIONARY_PRELOAD_SERVICE_FACTORY_H_
