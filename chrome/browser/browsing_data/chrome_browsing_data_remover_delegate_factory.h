// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class ChromeBrowsingDataRemoverDelegate;
class Profile;

class ChromeBrowsingDataRemoverDelegateFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns the singleton instance of ChromeBrowsingDataRemoverDelegateFactory.
  static ChromeBrowsingDataRemoverDelegateFactory* GetInstance();

  // Returns the ChromeBrowsingDataRemoverDelegate associated with |profile|.
  static ChromeBrowsingDataRemoverDelegate* GetForProfile(Profile* profile);

  ChromeBrowsingDataRemoverDelegateFactory(
      const ChromeBrowsingDataRemoverDelegateFactory&) = delete;
  ChromeBrowsingDataRemoverDelegateFactory& operator=(
      const ChromeBrowsingDataRemoverDelegateFactory&) = delete;

 private:
  friend base::NoDestructor<ChromeBrowsingDataRemoverDelegateFactory>;

  ChromeBrowsingDataRemoverDelegateFactory();
  ~ChromeBrowsingDataRemoverDelegateFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_FACTORY_H_
