// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash {
class FileSuggestKeyedService;

class FileSuggestKeyedServiceFactory : public ProfileKeyedServiceFactory {
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
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILE_SUGGEST_FILE_SUGGEST_KEYED_SERVICE_FACTORY_H_
