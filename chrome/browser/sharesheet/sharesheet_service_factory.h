// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace sharesheet {

class SharesheetService;

// Singleton that owns all SharesheetServices and associates them with Profile.
class SharesheetServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static SharesheetService* GetForProfile(Profile* profile);

  static SharesheetServiceFactory* GetInstance();

  SharesheetServiceFactory(const SharesheetServiceFactory&) = delete;
  SharesheetServiceFactory& operator=(const SharesheetServiceFactory&) = delete;

 private:
  friend base::NoDestructor<SharesheetServiceFactory>;

  SharesheetServiceFactory();
  ~SharesheetServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_FACTORY_H_
