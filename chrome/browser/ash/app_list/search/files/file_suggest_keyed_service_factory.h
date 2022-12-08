// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace app_list {
class FileSuggestKeyedService;

class FileSuggestKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static FileSuggestKeyedServiceFactory* GetInstance();

  FileSuggestKeyedServiceFactory(const FileSuggestKeyedServiceFactory&) =
      delete;
  FileSuggestKeyedServiceFactory& operator=(
      const FileSuggestKeyedServiceFactory&) = delete;
  ~FileSuggestKeyedServiceFactory() override;

  FileSuggestKeyedService* GetService(content::BrowserContext* context);

 private:
  friend base::NoDestructor<FileSuggestKeyedServiceFactory>;

  FileSuggestKeyedServiceFactory();

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_SUGGEST_KEYED_SERVICE_FACTORY_H_
